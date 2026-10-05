#ifndef SNAPPER_RENDER_STAGE_HIT_H_
#define SNAPPER_RENDER_STAGE_HIT_H_

#include <QPointF>
#include <QString>

#include <optional>

#include "base/frame.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// What was clicked: a layer, and for a doll, which piece.
struct StageHit final {
  LayerId layer;
  QString piece;
};

// The topmost thing drawn at point (output pixels of a frame rendered
// at scale), so clicks pick what is actually seen: see-through pixels
// of a drawing don't count. Effects and hidden layers are never hit.
std::optional<StageHit> HitTest(const Project& project, const Shot& shot,
                                Frame local, QPointF point, double scale,
                                ImageCache* cache);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_HIT_H_
