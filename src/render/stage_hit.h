#ifndef SNAPPER_RENDER_STAGE_HIT_H_
#define SNAPPER_RENDER_STAGE_HIT_H_

#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include <optional>
#include <vector>

#include "base/frame.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// What was clicked: a layer.
struct StageHit final {
  LayerId layer;
};

// The topmost thing drawn at point (output pixels of a frame rendered
// at scale), so clicks pick what is actually seen: see-through pixels
// of a picture don't count. Effects, hidden and locked layers are never
// hit.
std::optional<StageHit> HitTest(const Project& project, const Shot& shot,
                                Frame local, QPointF point, double scale,
                                ImageCache* cache);

// The outline of a picture, text or character layer on screen (a
// character's is one box around its shown parts); empty for effects.
QPolygonF LayerShape(const Project& project, const Shot& shot, LayerId layer,
                     Frame local, double scale, ImageCache* cache);

// Every picture, text or character layer whose box a box (output pixels
// at scale) touches. Effects, hidden and locked layers are never caught.
std::vector<StageHit> HitBox(const Project& project, const Shot& shot,
                             Frame local, const QRectF& box, double scale,
                             ImageCache* cache);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_HIT_H_
