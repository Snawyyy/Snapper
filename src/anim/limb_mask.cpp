#include <QRgb>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "anim/joint_snap.h"
#include "anim/shape_mesh.h"

namespace snapper {
namespace {

// Kept measurements before the memo starts over.
constexpr size_t kMaxMemo = 64;
// Joint colors kept: a dragged joint passes over many pixels.
constexpr size_t kMaxMemoColors = 4096;

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
    const bool is_empty = label == 0;
    if (is_empty) {
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
    LimbShape& shape = mask->shapes[index];
    shape.low = low[index];
    shape.high = high[index];
    shape.half_width = (side_high[index] - side_low[index]) / 2.0;
  }
}

}  // namespace

int LimbMask::LabelAt(QPointF at) const {
  assert(labels.size() == static_cast<size_t>(size.width()) *
                              static_cast<size_t>(size.height()));
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

std::shared_ptr<const LimbMask> LimbMemo::Of(const QImage& drawing) {
  assert(masks_.size() <= kMaxMemo);
  assert(drawing.width() >= 0);
  const std::int64_t key = drawing.cacheKey();
  const auto found = masks_.find(key);
  const bool is_known = found != masks_.end();
  if (is_known) {
    return found->second;
  }
  const bool is_full = masks_.size() >= kMaxMemo;
  if (is_full) {
    masks_.clear();
  }
  auto mask = std::make_shared<const LimbMask>(MeasureLimbs(drawing));
  masks_[key] = mask;
  return mask;
}

QColor LimbMemo::ColorAt(const QImage& drawing, QPointF at) {
  assert(colors_.size() <= kMaxMemoColors);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const ColorKey key{drawing.cacheKey(),
                     static_cast<int>(std::floor(at.x())),
                     static_cast<int>(std::floor(at.y()))};
  const auto found = colors_.find(key);
  const bool is_known = found != colors_.end();
  if (is_known) {
    return found->second;
  }
  const bool is_full = colors_.size() >= kMaxMemoColors;
  if (is_full) {
    colors_.clear();
  }
  const QColor color = JointColorAt(drawing, at);
  colors_[key] = color;
  return color;
}

}  // namespace snapper
