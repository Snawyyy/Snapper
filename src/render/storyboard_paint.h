#ifndef SNAPPER_RENDER_STORYBOARD_PAINT_H_
#define SNAPPER_RENDER_STORYBOARD_PAINT_H_

#include <QColor>
#include <QImage>

#include "base/frame.h"
#include "model/storyboard.h"
#include "render/image_cache.h"

namespace snapper {

// Onion skins: the cel before red, the one after green, as on a light
// table.
inline constexpr QColor kOnionBefore{220, 60, 60};
inline constexpr QColor kOnionAfter{60, 170, 90};

// Draws the cel of board showing at local over frame (the canvas at any
// scale), at the layer's opacity. Canvas space, no camera: the sketch is
// a sheet laid over what lies below it in the stack.
void PaintStoryboard(const StoryboardLayer& board, Frame local,
                     ImageCache* cache, QImage* frame);

// image in one color, keeping its alpha: an onion skin. Null for null.
QImage TintedCel(const QImage& image, QColor color);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STORYBOARD_PAINT_H_
