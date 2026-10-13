#include "render/comp_masks.h"

#include <algorithm>
#include <cassert>

namespace snapper {

QImage Masked(const QImage& picture, const Mask& mask) {
  assert(!picture.isNull());
  const bool is_whole = mask.empty();
  if (is_whole) {
    return picture;
  }
  QImage out = picture.copy();
  const int width = out.width();
  assert(mask.size() >= static_cast<size_t>(width) *
                            static_cast<size_t>(out.height()));
  for (int y = 0; y < out.height(); ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const int cover = mask[static_cast<size_t>(y) *
                                 static_cast<size_t>(width) +
                             static_cast<size_t>(x)];
      const QRgb p = row[x];
      row[x] = qRgba(qRed(p) * cover / 255, qGreen(p) * cover / 255,
                     qBlue(p) * cover / 255, qAlpha(p) * cover / 255);
    }
  }
  return out;
}

Mask GrownByOne(const Mask& mask, int width, int height) {
  assert(width > 0 && height > 0);
  assert(mask.size() ==
         static_cast<size_t>(width) * static_cast<size_t>(height));
  Mask grown(mask.size(), 0);
  const auto at = [width](int x, int y) {
    return static_cast<size_t>(y) * static_cast<size_t>(width) +
           static_cast<size_t>(x);
  };
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      std::uint8_t most = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = x + dx;
          const int ny = y + dy;
          const bool is_inside = nx >= 0 && nx < width && ny >= 0 &&
                                 ny < height;
          most = is_inside ? std::max(most, mask[at(nx, ny)]) : most;
        }
      }
      grown[at(x, y)] = most;
    }
  }
  return grown;
}

void ShowMask(const Mask& region, QImage* frame) {
  assert(frame != nullptr);
  frame->fill(Qt::black);
  const bool is_whole = region.empty();
  if (is_whole) {
    frame->fill(Qt::white);
    return;
  }
  const int width = frame->width();
  assert(region.size() >= static_cast<size_t>(width) *
                              static_cast<size_t>(frame->height()));
  for (int y = 0; y < frame->height(); ++y) {
    auto* row = reinterpret_cast<QRgb*>(frame->scanLine(y));
    for (int x = 0; x < width; ++x) {
      const int cover = region[static_cast<size_t>(y) *
                                   static_cast<size_t>(width) +
                               static_cast<size_t>(x)];
      row[x] = qRgb(cover, cover, cover);
    }
  }
}

}  // namespace snapper
