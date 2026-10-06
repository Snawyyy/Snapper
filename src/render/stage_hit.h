#ifndef SNAPPER_RENDER_STAGE_HIT_H_
#define SNAPPER_RENDER_STAGE_HIT_H_

#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>

#include <optional>
#include <vector>

#include "base/frame.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// What was clicked: a layer, and for a doll, which piece.
struct StageHit final {
  LayerId layer;
  QString piece;
};

// The topmost solid piece of a posed doll under point, where world maps
// the doll's space to the point's space.
std::optional<QString> HitDollPiece(const Doll& doll, const DollLayer& layer,
                                    Frame local, const QTransform& world,
                                    QPointF point, ImageCache* cache);

// The topmost thing drawn at point (output pixels of a frame rendered
// at scale), so clicks pick what is actually seen: see-through pixels
// of a drawing don't count. Effects and hidden layers are never hit.
std::optional<StageHit> HitTest(const Project& project, const Shot& shot,
                                Frame local, QPointF point, double scale,
                                ImageCache* cache);

// The outline of a picture, text or whole doll layer on screen (a doll's
// is one box around all its pieces); empty for effects.
QPolygonF LayerShape(const Project& project, const Shot& shot, LayerId layer,
                     Frame local, double scale, ImageCache* cache);

// Everything a box (output pixels at scale) touches: each doll piece
// whose drawing overlaps it, and each other picture or text layer whose
// box does. Effects and hidden layers are never caught.
std::vector<StageHit> HitBox(const Project& project, const Shot& shot,
                             Frame local, const QRectF& box, double scale,
                             ImageCache* cache);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_HIT_H_
