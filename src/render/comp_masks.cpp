#include "render/comp_masks.h"

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
