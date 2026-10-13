#ifndef SNAPPER_RENDER_COMP_MASKS_H_
#define SNAPPER_RENDER_COMP_MASKS_H_

#include <QImage>

#include "render/comp_effects.h"

namespace snapper {

// picture with every pixel weighed by mask (empty: picture as is).
QImage Masked(const QImage& picture, const Mask& mask);
// Paints what region covers white over black: the K view.
void ShowMask(const Mask& region, QImage* frame);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_MASKS_H_
