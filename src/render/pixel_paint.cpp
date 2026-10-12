#include "render/pixel_paint.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>

namespace snapper {
namespace {

// How round a dab is: a pixel is in when its middle is within this share
// of the radius squared, so size 3 is a plus and bigger sizes look round.
constexpr double kRoundness = 0.85;

QRgb PixelOf(QColor color) {
  assert(color.isValid());
  const bool is_clear = color.alpha() == 0;
  return is_clear ? qRgba(0, 0, 0, 0) : color.rgba();
}

// Fully clear pixels are one color, whatever their hidden rgb.
bool IsSame(QRgb pixel, QRgb wanted) {
  const bool are_clear = qAlpha(pixel) == 0 && qAlpha(wanted) == 0;
  return are_clear || pixel == wanted;
}

void SetPixel(QImage* image, int x, int y, QRgb pixel) {
  assert(image != nullptr);
  assert(image->format() == QImage::Format_ARGB32);
  const bool is_inside =
      x >= 0 && y >= 0 && x < image->width() && y < image->height();
  if (is_inside) {
    reinterpret_cast<QRgb*>(image->scanLine(y))[x] = pixel;
  }
}

}  // namespace

std::vector<QPoint> DabPixels(int size) {
  assert(size >= 1);
  assert(size <= kMaxBrushSize);
  const double middle = (size - 1) / 2.0;
  const double radius = size / 2.0;
  const int shift = (size - 1) / 2;
  std::vector<QPoint> pixels;
  for (int j = 0; j < size; ++j) {
    for (int i = 0; i < size; ++i) {
      const double dx = i - middle;
      const double dy = j - middle;
      const bool is_in = dx * dx + dy * dy <= radius * radius * kRoundness ||
                         size == 1;
      if (is_in) {
        pixels.emplace_back(i - shift, j - shift);
      }
    }
  }
  assert(!pixels.empty());
  return pixels;
}

void PaintDab(QImage* image, QPoint center, int size, QColor color) {
  assert(image != nullptr);
  assert(size >= 1 && size <= kMaxBrushSize);
  const QRgb pixel = PixelOf(color);
  for (const QPoint& at : DabPixels(size)) {
    SetPixel(image, center.x() + at.x(), center.y() + at.y(), pixel);
  }
}

void PaintLine(QImage* image, QPoint from, QPoint to, int size, QColor color) {
  assert(image != nullptr);
  assert(size >= 1 && size <= kMaxBrushSize);
  const QRgb pixel = PixelOf(color);
  const std::vector<QPoint> dab = DabPixels(size);
  // Bresenham's line: one dab on every pixel the line crosses.
  const int dx = std::abs(to.x() - from.x());
  const int dy = -std::abs(to.y() - from.y());
  const int step_x = from.x() < to.x() ? 1 : -1;
  const int step_y = from.y() < to.y() ? 1 : -1;
  int error = dx + dy;
  QPoint at = from;
  const int most = dx - dy + 1;
  for (int count = 0; count <= most; ++count) {
    for (const QPoint& offset : dab) {
      SetPixel(image, at.x() + offset.x(), at.y() + offset.y(), pixel);
    }
    if (at == to) {
      break;
    }
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      at.rx() += step_x;
    }
    if (twice <= dx) {
      error += dx;
      at.ry() += step_y;
    }
  }
}

bool FloodFill(QImage* image, QPoint at, QColor color) {
  assert(image != nullptr);
  assert(image->format() == QImage::Format_ARGB32);
  const bool is_inside = image->rect().contains(at);
  if (!is_inside) {
    return false;
  }
  const QRgb fill = PixelOf(color);
  const QRgb wanted = image->pixel(at);
  if (IsSame(wanted, fill)) {
    return false;
  }
  const int width = image->width();
  const int height = image->height();
  std::vector<QPoint> stack{at};
  // Each pixel is filled once and pushed at most four times.
  const size_t most =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4 + 1;
  for (size_t round = 0; round < most && !stack.empty(); ++round) {
    const QPoint seed = stack.back();
    stack.pop_back();
    auto* row = reinterpret_cast<QRgb*>(image->scanLine(seed.y()));
    if (!IsSame(row[seed.x()], wanted)) {
      continue;
    }
    int left = seed.x();
    while (left > 0 && IsSame(row[left - 1], wanted)) {
      --left;
    }
    int right = seed.x();
    while (right + 1 < width && IsSame(row[right + 1], wanted)) {
      ++right;
    }
    for (int x = left; x <= right; ++x) {
      row[x] = fill;
      for (const int y : {seed.y() - 1, seed.y() + 1}) {
        const bool is_row = y >= 0 && y < height;
        if (is_row && IsSame(image->pixel(x, y), wanted)) {
          stack.emplace_back(x, y);
        }
      }
    }
  }
  return true;
}

}  // namespace snapper
