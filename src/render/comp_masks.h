#ifndef SNAPPER_RENDER_COMP_MASKS_H_
#define SNAPPER_RENDER_COMP_MASKS_H_

#include <QImage>

#include "render/comp_effects.h"

namespace snapper {

// picture with every pixel weighed by mask (empty: picture as is).
QImage Masked(const QImage& picture, const Mask& mask);
// mask spread one pixel each way (the most of each 3 by 3), so an effect
// that changes both sides of an edge reaches the outside one too.
Mask GrownByOne(const Mask& mask, int width, int height);
// Paints what region covers white over black: the K view.
void ShowMask(const Mask& region, QImage* frame);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_MASKS_H_
