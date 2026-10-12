#include "render/upscale.h"

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

// One Scale2x pass: each pixel P becomes four, and a corner takes the
// color of the two neighbours that meet there when they match and the
// other two do not (a diagonal edge), else P's own.
QImage Scale2x(const QImage& source) {
  assert(source.format() == QImage::Format_ARGB32_Premultiplied);
  assert(!source.isNull());
  const int width = source.width();
  const int height = source.height();
  QImage out(width * 2, height * 2, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < height; ++y) {
    const auto* above = reinterpret_cast<const QRgb*>(
        source.constScanLine(std::max(0, y - 1)));
    const auto* row = reinterpret_cast<const QRgb*>(source.constScanLine(y));
    const auto* below = reinterpret_cast<const QRgb*>(
        source.constScanLine(std::min(height - 1, y + 1)));
    auto* top = reinterpret_cast<QRgb*>(out.scanLine(y * 2));
    auto* bottom = reinterpret_cast<QRgb*>(out.scanLine(y * 2 + 1));
    for (int x = 0; x < width; ++x) {
      const QRgb p = row[x];
      const QRgb a = above[x];
      const QRgb d = below[x];
      const QRgb c = row[std::max(0, x - 1)];
      const QRgb b = row[std::min(width - 1, x + 1)];
      const bool is_edge = c != b && a != d;
      top[x * 2] = is_edge && c == a ? a : p;
      top[x * 2 + 1] = is_edge && a == b ? b : p;
      bottom[x * 2] = is_edge && d == c ? c : p;
      bottom[x * 2 + 1] = is_edge && b == d ? d : p;
    }
  }
  return out;
}

}  // namespace

QImage UpscaleDrawing(const QImage& source, int times) {
  assert(times == 2 || times == kMaxUpscale);
  assert(!source.isNull());
  const QImage premultiplied =
      source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QImage out = Scale2x(premultiplied);
  for (int done = 2; done < times; done *= 2) {
    out = Scale2x(out);
  }
  assert(out.width() == source.width() * times);
  return out;
}

int UpscaleTimes(QSize size, double scale) {
  assert(scale >= 0.0);
  assert(size.width() >= 0 && size.height() >= 0);
  const bool is_large =
      std::max(size.width(), size.height()) > kLargeDrawingSide;
  const bool is_zoomed = scale > 1.0;
  return is_zoomed && !is_large ? kMaxUpscale : 2;
}

}  // namespace snapper
