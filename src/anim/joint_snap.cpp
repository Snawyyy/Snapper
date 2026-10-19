#include "anim/joint_snap.h"

#include <QRectF>
#include <QRgb>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <vector>

#include "anim/guides.h"

namespace snapper {
namespace {

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
// How far, in drawing pixels, a joint circle looks for its color.
constexpr int kJointColorReach = 4;
// A pivot this close, in drawing pixels, to an end counts as on it.
constexpr double kOnEnd = 1.0;

// The shape labelled label (from 1).
const LimbShape& ShapeAt(const LimbMask& mask, int label) {
  assert(label > 0);
  assert(static_cast<size_t>(label) <= mask.shapes.size());
  return mask.shapes[static_cast<size_t>(label - 1)];
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
  const bool is_found = found.has_value();
  if (!is_found) {
    return found;
  }
  // The circle's middle sits where the middle line leaves the drawing,
  // so a joint circle there caps the end instead of ending on it.
  const QPointF out = -inward;
  const QPointF to_end = out * ReachAlong(mask, found->spot, out, label);
  found->spot += to_end;
  found->edge_a += to_end;
  found->edge_b += to_end;
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
  const bool has_overlap = overlap.has_value();
  if (has_overlap) {
    spots.push_back(*overlap);
  }
  return spots;
}

}  // namespace

std::optional<ShapeSnap> WidthMiddleAt(const LimbMask& mask, QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(mask.size.width() >= 0);
  const int label = mask.LabelAt(at);
  const bool is_off = label == 0;
  if (is_off) {
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
  snap.middle_a = shape.center + shape.along * shape.low;
  snap.middle_b = shape.center + shape.along * shape.high;
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
      const bool has_end = end.has_value();
      if (has_end) {
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
  const bool is_empty = own_label == 0 || parent_label == 0;
  if (is_empty) {
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
  const bool is_parallel = std::abs(cross) < kMinCross;
  if (is_parallel) {
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
  PivotSnap best{wanted, std::nullopt, false, std::nullopt};
  double nearest = reach;
  for (const ShapeSnap& spot : SpotsOf(own, parent, parent_to_own)) {
    const double gap = Distance(spot.spot, wanted);
    const bool is_nearer = gap <= nearest;
    if (is_nearer) {
      best = PivotSnap{spot.spot, spot, true, std::nullopt};
      nearest = gap;
    }
  }
  const bool is_spot = best.is_snapped;
  if (is_spot) {
    return best;
  }
  const auto width = WidthMiddleAt(own, wanted);
  const double width_gap = width ? Distance(width->spot, wanted) : reach + 1.0;
  const bool is_width = width_gap <= reach;
  if (is_width) {
    best = PivotSnap{width->spot, *width, true, std::nullopt};
  }
  const GuideSnap box =
      SnapToGuides(QRectF(wanted, QSizeF(0.0, 0.0)),
                   {QRectF(QPointF(), QSizeF(own.size))}, reach, {});
  const double box_gap = std::hypot(box.shift.x(), box.shift.y());
  const bool is_box = !box.lines.empty() && box_gap < width_gap;
  if (is_box) {
    best = PivotSnap{wanted + box.shift, std::nullopt, true, std::nullopt};
  }
  const bool is_shape = best.shape.has_value();
  if (!is_shape) {
    best.near = NearestShapeSnap(own, parent, parent_to_own, wanted);
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

double JointRadius(const LimbMask& mask, QPointF pivot) {
  assert(std::isfinite(pivot.x()) && std::isfinite(pivot.y()));
  assert(mask.size.width() >= 0);
  // An end's pivot sits on the drawing's edge, where the width runs out.
  for (const ShapeSnap& end : EndCentersOf(mask)) {
    const bool is_on_end = Distance(end.spot, pivot) <= kOnEnd;
    if (is_on_end) {
      return end.radius;
    }
  }
  const auto width = WidthMiddleAt(mask, pivot);
  return width ? Distance(width->edge_a, width->edge_b) / 2.0 : 0.0;
}

QColor JointColorAt(const QImage& drawing, QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(kJointColorReach > 0);
  // pixel() reads any format; a solid pixel is the same premultiplied.
  std::map<QRgb, int> counts;
  const int cx = static_cast<int>(std::floor(at.x()));
  const int cy = static_cast<int>(std::floor(at.y()));
  for (int y = cy - kJointColorReach; y <= cy + kJointColorReach; ++y) {
    for (int x = cx - kJointColorReach; x <= cx + kJointColorReach; ++x) {
      const bool is_in = drawing.valid(x, y);
      const QRgb pixel = is_in ? drawing.pixel(x, y) : 0;
      const bool is_solid = is_in && qAlpha(pixel) == 255;
      if (is_solid) {
        ++counts[pixel];
      }
    }
  }
  QRgb best = qRgb(0, 0, 0);
  int most = 0;
  for (const auto& [pixel, count] : counts) {
    const bool is_more = count > most;
    if (is_more) {
      best = pixel;
      most = count;
    }
  }
  return QColor::fromRgba(best);
}

}  // namespace snapper
