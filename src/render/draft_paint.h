#ifndef SNAPPER_RENDER_DRAFT_PAINT_H_
#define SNAPPER_RENDER_DRAFT_PAINT_H_

#include <QColor>
#include <QImage>

#include "base/frame.h"
#include "model/draft.h"
#include "render/image_cache.h"

namespace snapper {

// Onion skins: the cel before red, the one after green, as on a light
// table.
inline constexpr QColor kOnionBefore{220, 60, 60};
inline constexpr QColor kOnionAfter{60, 170, 90};

// Draws the cel of draft showing at local over frame (the canvas at any
// scale), at the draft's opacity, when its eye is open. Canvas space,
// no camera: the draft is a sheet laid over the screen.
void PaintDraft(const Draft& draft, Frame local, ImageCache* cache,
                QImage* frame);

// image in one color, keeping its alpha: an onion skin. Null for null.
QImage TintedCel(const QImage& image, QColor color);

}  // namespace snapper

#endif  // SNAPPER_RENDER_DRAFT_PAINT_H_
