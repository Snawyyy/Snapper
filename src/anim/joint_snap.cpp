#include "anim/joint_snap.h"

#include <QRectF>
#include <QRgb>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "anim/guides.h"
#include "anim/shape_mesh.h"

namespace snapper {
namespace {

// A drawing past this many pixels is not measured (nothing catches).
constexpr std::int64_t kMaxMaskPixels = 8192LL * 8192LL;
// A shape needs this many pixels before its ends count: specks have
// none.
constexpr int kMinEndPixels = 64;
// How far apart, in drawing pixels, a width is sampled along its line.
constexpr double kStep = 0.125;
// Two center lines closer than this to parallel (the sine of the angle
// between them) cross nowhere useful.
constexpr double kMinCross = 0.1;
// End circles are found by this many rounds of measuring in from the
// tip.
constexpr int kEndRounds = 4;
// Kept measurements before the memo starts over.
constexpr size_t kMaxMemo = 64;

// Running sums of a shape's pixel centers, for its middle and spread.
struct Sums final {
  double n = 0.0;
  double x = 0.0;
  double y = 0.0;
  double xx = 0.0;
  double yy = 0.0;
  double xy = 0.0;
};

// Where pixel (x, y) of a drawing width wide is in its row by row list.
size_t Cell(int x, int y, int width) {
  assert(x >= 0 && y >= 0 && x < width);
  assert(static_cast<std::int64_t>(y) * width <= kMaxMaskPixels);
  return static_cast<size_t>(y) * static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

// The pixel center of cell at in a drawing width wide.
QPointF CenterOf(size_t at, int width) {
  assert(width > 0);
  assert(at <= static_cast<size_t>(kMaxMaskPixels));
  const auto columns = static_cast<size_t>(width);
  return QPointF(static_cast<double>(at % columns) + 0.5,
                 static_cast<double>(at / columns) + 0.5);
}

// The shape labelled label (from 1).
const LimbShape& ShapeAt(const LimbMask& mask, int label) {
  assert(label > 0);
  assert(static_cast<size_t>(label) <= mask.shapes.size());
  return mask.shapes[static_cast<size_t>(label - 1)];
}

// 1 where a pixel of image is visible.
std::vector<std::uint8_t> VisibleOf(const QImage& image) {
  assert(!image.isNull());
  assert(image.format() == QImage::Format_ARGB32_Premultiplied);
  const int width = image.width();
  std::vector<std::uint8_t> visible(
      static_cast<size_t>(width) * static_cast<size_t>(image.height()), 0);
  for (int y = 0; y < image.height(); ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < width; ++x) {
      visible[Cell(x, y, width)] = qAlpha(row[x]) >= kMeshAlpha ? 1 : 0;
    }
  }
  return visible;
}

// 1 where a pixel is empty and joined to the drawing's border through
// empty pixels; empty pixels that are not are holes, closed in.
std::vector<std::uint8_t> OutsideOf(const std::vector<std::uint8_t>& visible,
                                    int width, int height) {
  assert(width > 0 && height > 0);
  assert(visible.size() ==
         static_cast<size_t>(width) * static_cast<size_t>(height));
  std::vector<std::uint8_t> outside(visible.size(), 0);
  std::vector<int> stack;
  const auto push = [&](int x, int y) {
    const bool is_in = x >= 0 && y >= 0 && x < width && y < height;
    if (!is_in) {
      return;
    }
    const size_t at = Cell(x, y, width);
    const bool is_new = visible[at] == 0 && outside[at] == 0;
    if (is_new) {
      outside[at] = 1;
      stack.push_back(y * width + x);
    }
  };
  for (int x = 0; x < width; ++x) {
    push(x, 0);
    push(x, height - 1);
  }
  for (int y = 0; y < height; ++y) {
    push(0, y);
    push(width - 1, y);
  }
  while (!stack.empty()) {
    const int at = stack.back();
    stack.pop_back();
    const int x = at % width;
    const int y = at / width;
    push(x + 1, y);
    push(x - 1, y);
    push(x, y + 1);
    push(x, y - 1);
  }
  return outside;
}

// Labels every solid pixel joined to start (8 ways) with label, adding
// their centers to sums.
void Fill(size_t start, std::int32_t label,
          const std::vector<std::uint8_t>& solid, int width, LimbMask* mask,
          Sums* sums) {
  assert(mask != nullptr && sums != nullptr);
  assert(label > 0);
  const int height = mask->size.height();
  std::vector<size_t> stack{start};
  mask->labels[start] = label;
  while (!stack.empty()) {
    const size_t at = stack.back();
    stack.pop_back();
    const QPointF center = CenterOf(at, width);
    sums->n += 1.0;
    sums->x += center.x();
    sums->y += center.y();
    sums->xx += center.x() * center.x();
    sums->yy += center.y() * center.y();
    sums->xy += center.x() * center.y();
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int x = static_cast<int>(center.x()) + dx;
        const int y = static_cast<int>(center.y()) + dy;
        const bool is_in = x >= 0 && y >= 0 && x < width && y < height;
        const size_t next = is_in ? Cell(x, y, width) : at;
        const bool is_new =
            is_in && solid[next] != 0 && mask->labels[next] == 0;
        if (is_new) {
          mask->labels[next] = label;
          stack.push_back(next);
        }
      }
    }
  }
}

// A shape's middle and long direction from its sums.
LimbShape ShapeOf(const Sums& sums) {
  assert(sums.n > 0.0);
  assert(std::isfinite(sums.xx) && std::isfinite(sums.yy));
  LimbShape shape;
  const double mx = sums.x / sums.n;
  const double my = sums.y / sums.n;
  const double cxx = sums.xx / sums.n - mx * mx;
  const double cyy = sums.yy / sums.n - my * my;
  const double cxy = sums.xy / sums.n - mx * my;
  const double angle = 0.5 * std::atan2(2.0 * cxy, cxx - cyy);
  shape.center = QPointF(mx, my);
  shape.along = QPointF(std::cos(angle), std::sin(angle));
  shape.pixels = static_cast<int>(sums.n);
  return shape;
}

// Fills in how far each shape reaches along and across.
void MeasureReach(LimbMask* mask) {
  assert(mask != nullptr);
  assert(mask->labels.size() == static_cast<size_t>(mask->size.width()) *
                                    static_cast<size_t>(mask->size.height()));
  const int width = mask->size.width();
  const size_t count = mask->shapes.size();
  std::vector<double> low(count, std::numeric_limits<double>::max());
  std::vector<double> high(count, std::numeric_limits<double>::lowest());
  std::vector<double> side_low(count, std::numeric_limits<double>::max());
  std::vector<double> side_high(count, std::numeric_limits<double>::lowest());
  for (size_t at = 0; at < mask->labels.size(); ++at) {
    const std::int32_t label = mask->labels[at];
    if (label == 0) {
      continue;
    }
    const auto index = static_cast<size_t>(label - 1);
    const LimbShape& shape = mask->shapes[index];
    const QPointF from = CenterOf(at, width) - shape.center;
    const double along = QPointF::dotProduct(from, shape.along);
    const double across =
        QPointF::dotProduct(from, QPointF(-shape.along.y(), shape.along.x()));
    low[index] = std::min(low[index], along - 0.5);
    high[index] = std::max(high[index], along + 0.5);
    side_low[index] = std::min(side_low[index], across - 0.5);
    side_high[index] = std::max(side_high[index], across + 0.5);
  }
  for (size_t index = 0; index < count; ++index) {
    mask->shapes[index].low = low[index];
    mask->shapes[index].high = high[index];
    mask->shapes[index].half_width = (side_high[index] - side_low[index]) / 2.0;
  }
}

// How far from at along direction (unit) the shape labelled label goes
// on, sampled every kStep.
double ReachAlong(const LimbMask& mask, QPointF at, QPointF direction,
                  int label) {
  assert(label > 0);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const int steps =
      static_cast<int>((mask.size.width() + mask.size.height()) / kStep) + 4;
  for (int step = 1; step <= steps; ++step) {
    const bool is_out = mask.LabelAt(at + direction * (step * kStep)) != label;
    if (is_out) {
      return (step - 0.5) * kStep;
    }
  }
  return steps * kStep;
}

// The label of mask's biggest shape; 0 when it has none.
int BiggestOf(const LimbMask& mask) {
  assert(mask.shapes.size() < static_cast<size_t>(kMaxMaskPixels));
  assert(mask.labels.size() <= static_cast<size_t>(kMaxMaskPixels));
  int best = 0;
  int pixels = 0;
  for (size_t index = 0; index < mask.shapes.size(); ++index) {
    const bool is_bigger = mask.shapes[index].pixels > pixels;
    if (is_bigger) {
      pixels = mask.shapes[index].pixels;
      best = static_cast<int>(index) + 1;
    }
  }
  return best;
}

// One end circle of shape (label), from its tip at the far end along
// `along` (is_high) or the near one.
std::optional<ShapeSnap> EndCenterOf(const LimbMask& mask, int label,
                                     bool is_high) {
  assert(label > 0 && label <= static_cast<int>(mask.shapes.size()));
  assert(mask.labels.size() == static_cast<size_t>(mask.size.width()) *
                                   static_cast<size_t>(mask.size.height()));
  const LimbShape& shape = ShapeAt(mask, label);
  const QPointF tip =
      shape.center + shape.along * (is_high ? shape.high : shape.low);
  const QPointF inward = shape.along * (is_high ? -1.0 : 1.0);
  const double longest = (shape.high - shape.low) / 2.0;
  double radius = std::min(shape.half_width, longest);
  std::optional<ShapeSnap> found;
  for (int round = 0; round < kEndRounds; ++round) {
    const QPointF probe = tip + inward * radius;
    const bool is_on = mask.LabelAt(probe) == label;
    const auto width = is_on ? WidthMiddleAt(mask, probe) : std::nullopt;
    if (!width) {
      break;
    }
    const QPointF span = width->edge_b - width->edge_a;
    radius = std::min(std::hypot(span.x(), span.y()) / 2.0, longest);
    found = *width;
    found->kind = ShapeSnapKind::kEndCenter;
    found->radius = radius;
  }
  return found;
}

double Distance(QPointF a, QPointF b) {
  assert(std::isfinite(a.x()) && std::isfinite(a.y()));
  assert(std::isfinite(b.x()) && std::isfinite(b.y()));
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// Every spot snap of own: its end centers and the overlap center.
std::vector<ShapeSnap> SpotsOf(const LimbMask& own, const LimbMask* parent,
                               const QTransform& parent_to_own) {
  assert(own.shapes.size() < static_cast<size_t>(kMaxMaskPixels));
  assert(parent_to_own.isAffine());
  std::vector<ShapeSnap> spots = EndCentersOf(own);
  const auto overlap = parent != nullptr
                           ? OverlapCenter(own, *parent, parent_to_own)
                           : std::nullopt;
  if (overlap) {
    spots.push_back(*overlap);
  }
  return spots;
}

}  // namespace

int LimbMask::LabelAt(QPointF at) const {
  assert(labels.size() ==
         static_cast<size_t>(size.width()) * static_cast<size_t>(size.height()));
  assert(size.width() >= 0 && size.height() >= 0);
  const bool is_finite = std::isfinite(at.x()) && std::isfinite(at.y());
  const double x = is_finite ? std::floor(at.x()) : -1.0;
  const double y = is_finite ? std::floor(at.y()) : -1.0;
  const bool is_in = x >= 0.0 && y >= 0.0 && x < size.width() &&
                     y < size.height();
  return is_in ? labels[static_cast<size_t>(y) *
                            static_cast<size_t>(size.width()) +
                        static_cast<size_t>(x)]
               : 0;
}

LimbMask MeasureLimbs(const QImage& drawing) {
  assert(drawing.width() >= 0 && drawing.height() >= 0);
  assert(drawing.isNull() || drawing.width() > 0);
  LimbMask mask;
  mask.size = QSize(0, 0);
  const std::int64_t pixels =
      static_cast<std::int64_t>(drawing.width()) * drawing.height();
  const bool is_measurable =
      !drawing.isNull() && pixels > 0 && pixels <= kMaxMaskPixels;
  if (!is_measurable) {
    return mask;
  }
  const QImage image =
      drawing.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const int width = image.width();
  const int height = image.height();
  const std::vector<std::uint8_t> visible = VisibleOf(image);
  const std::vector<std::uint8_t> outside = OutsideOf(visible, width, height);
  std::vector<std::uint8_t> solid(visible.size(), 0);
  for (size_t at = 0; at < solid.size(); ++at) {
    solid[at] = visible[at] != 0 || outside[at] == 0 ? 1 : 0;
  }
  mask.size = QSize(width, height);
  mask.labels.assign(solid.size(), 0);
  for (size_t at = 0; at < solid.size(); ++at) {
    const bool is_new = solid[at] != 0 && mask.labels[at] == 0;
    if (is_new) {
      Sums sums;
      const auto label = static_cast<std::int32_t>(mask.shapes.size() + 1);
      Fill(at, label, solid, width, &mask, &sums);
      mask.shapes.push_back(ShapeOf(sums));
    }
  }
  MeasureReach(&mask);
  return mask;
}

std::optional<ShapeSnap> WidthMiddleAt(const LimbMask& mask, QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(mask.size.width() >= 0);
  const int label = mask.LabelAt(at);
  if (label == 0) {
    return std::nullopt;
  }
  const LimbShape& shape = ShapeAt(mask, label);
  const QPointF across(-shape.along.y(), shape.along.x());
  const double plus = ReachAlong(mask, at, across, label);
  const double minus = ReachAlong(mask, at, -across, label);
  ShapeSnap snap;
  snap.kind = ShapeSnapKind::kWidthMiddle;
  snap.edge_a = at - across * minus;
  snap.edge_b = at + across * plus;
  snap.spot = (snap.edge_a + snap.edge_b) / 2.0;
  return snap;
}

std::vector<ShapeSnap> EndCentersOf(const LimbMask& mask) {
  assert(mask.shapes.size() < static_cast<size_t>(kMaxMaskPixels));
  assert(mask.labels.size() <= static_cast<size_t>(kMaxMaskPixels));
  std::vector<ShapeSnap> ends;
  for (size_t index = 0; index < mask.shapes.size(); ++index) {
    const bool is_shape = mask.shapes[index].pixels >= kMinEndPixels;
    if (!is_shape) {
      continue;
    }
    for (const bool is_high : {false, true}) {
      const auto end = EndCenterOf(mask, static_cast<int>(index) + 1, is_high);
      if (end) {
        ends.push_back(*end);
      }
    }
  }
  return ends;
}

std::optional<ShapeSnap> OverlapCenter(const LimbMask& own,
                                       const LimbMask& parent,
                                       const QTransform& parent_to_own) {
  assert(parent_to_own.isAffine());
  assert(own.shapes.size() < static_cast<size_t>(kMaxMaskPixels));
  const int own_label = BiggestOf(own);
  const int parent_label = BiggestOf(parent);
  if (own_label == 0 || parent_label == 0) {
    return std::nullopt;
  }
  const LimbShape& mine = ShapeAt(own, own_label);
  const LimbShape& theirs = ShapeAt(parent, parent_label);
  const QPointF their_center = parent_to_own.map(theirs.center);
  const QPointF their_way =
      parent_to_own.map(theirs.center + theirs.along) - their_center;
  const double length = std::hypot(their_way.x(), their_way.y());
  const double cross =
      length > 1e-9 ? (mine.along.x() * their_way.y() -
                       mine.along.y() * their_way.x()) /
                          length
                    : 0.0;
  if (std::abs(cross) < kMinCross) {
    return std::nullopt;
  }
  const QPointF gap = their_center - mine.center;
  const double t =
      (gap.x() * their_way.y() - gap.y() * their_way.x()) / length / cross;
  const QPointF spot = mine.center + mine.along * t;
  bool is_invertible = false;
  const QPointF back = parent_to_own.inverted(&is_invertible).map(spot);
  const bool is_inside = own.LabelAt(spot) == own_label && is_invertible &&
                         parent.LabelAt(back) == parent_label;
  if (!is_inside) {
    return std::nullopt;
  }
  ShapeSnap snap = WidthMiddleAt(own, spot).value_or(ShapeSnap{});
  snap.kind = ShapeSnapKind::kOverlapCenter;
  snap.spot = spot;
  snap.radius = 0.0;
  return snap;
}

PivotSnap SnapPivot(const LimbMask& own, const LimbMask* parent,
                    const QTransform& parent_to_own, QPointF wanted,
                    double reach) {
  assert(std::isfinite(wanted.x()) && std::isfinite(wanted.y()));
  assert(reach >= 0.0);
  PivotSnap best{wanted, std::nullopt, false};
  double nearest = reach;
  for (const ShapeSnap& spot : SpotsOf(own, parent, parent_to_own)) {
    const double gap = Distance(spot.spot, wanted);
    const bool is_nearer = gap <= nearest;
    if (is_nearer) {
      best = PivotSnap{spot.spot, spot, true};
      nearest = gap;
    }
  }
  if (best.is_snapped) {
    return best;
  }
  const auto width = WidthMiddleAt(own, wanted);
  const double width_gap = width ? Distance(width->spot, wanted) : reach + 1.0;
  if (width_gap <= reach) {
    best = PivotSnap{width->spot, *width, true};
  }
  const GuideSnap box =
      SnapToGuides(QRectF(wanted, QSizeF(0.0, 0.0)),
                   {QRectF(QPointF(), QSizeF(own.size))}, reach, {});
  const double box_gap = std::hypot(box.shift.x(), box.shift.y());
  const bool is_box = !box.lines.empty() && box_gap < width_gap;
  if (is_box) {
    best = PivotSnap{wanted + box.shift, std::nullopt, true};
  }
  return best;
}

std::optional<ShapeSnap> NearestShapeSnap(const LimbMask& own,
                                          const LimbMask* parent,
                                          const QTransform& parent_to_own,
                                          QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(parent_to_own.isAffine());
  std::optional<ShapeSnap> best;
  double nearest = std::numeric_limits<double>::max();
  for (const ShapeSnap& spot : SpotsOf(own, parent, parent_to_own)) {
    const double reach = spot.radius > 0.0
                             ? spot.radius
                             : Distance(spot.edge_a, spot.edge_b) / 2.0;
    const double gap = Distance(spot.spot, at);
    const bool is_nearer = gap <= reach && gap < nearest;
    if (is_nearer) {
      best = spot;
      nearest = gap;
    }
  }
  return best ? best : WidthMiddleAt(own, at);
}

std::shared_ptr<const LimbMask> LimbMemo::Of(const QImage& drawing) {
  assert(masks_.size() <= kMaxMemo);
  assert(drawing.width() >= 0);
  const std::int64_t key = drawing.cacheKey();
  const auto found = masks_.find(key);
  if (found != masks_.end()) {
    return found->second;
  }
  if (masks_.size() >= kMaxMemo) {
    masks_.clear();
  }
  auto mask = std::make_shared<const LimbMask>(MeasureLimbs(drawing));
  masks_[key] = mask;
  return mask;
}

}  // namespace snapper
