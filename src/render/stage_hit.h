#ifndef SNAPPER_RENDER_STAGE_HIT_H_
#define SNAPPER_RENDER_STAGE_HIT_H_

#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>

#include <optional>
#include <vector>

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// What was clicked: a layer, and for a doll, which piece.
struct StageHit final {
  LayerId layer;
  QString piece;
};

// The topmost solid piece of a doll posed by poses under point, where
// world maps the doll's space to the point's space.
std::optional<QString> HitDollPiece(const Doll& doll, const PoseMap& poses,
                                    const QTransform& world, QPointF point,
                                    ImageCache* cache);

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

// How far, in screen pixels, the lean handle sits out from the box.
constexpr double kLeanGap = 12.0;

// Where a doll layer's lean handle sits on screen: just out from the
// middle of the right side of its box (LayerShape). Nothing for other
// layers.
std::optional<QPointF> LeanHandle(const Project& project, const Shot& shot,
                                  LayerId layer, Frame local, double scale,
                                  ImageCache* cache);

// Everything a box (output pixels at scale) touches: each doll piece
// whose drawing overlaps it, and each other picture or text layer whose
// box does. Effects and hidden layers are never caught.
std::vector<StageHit> HitBox(const Project& project, const Shot& shot,
                             Frame local, const QRectF& box, double scale,
                             ImageCache* cache);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_HIT_H_
