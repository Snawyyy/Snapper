#ifndef SNAPPER_ANIM_DOLL_COLLIDERS_H_
#define SNAPPER_ANIM_DOLL_COLLIDERS_H_

#include <QRectF>
#include <QString>
#include <QTransform>

#include <map>
#include <vector>

#include "anim/collider.h"
#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// One collider as it pushes one piece at a frame, in shot space: its
// edges, the box around them, and for every grid point of the pushed
// piece its ghost, where that point would be had it kept the place
// against the collider it has as drawn (both pieces at rest). See
// PushedFromRest.
struct PushField final {
  std::vector<ColliderEdge> edges;
  QRectF box;
  std::vector<QPointF> ghosts;
};

// Whether collider pushes piece: collider is the doll's piece drawn
// behind piece (a lower draw order), has an outline, and does not spare
// it. Only a piece with a warp grid can be pushed.
bool IsPushing(const RigPiece& collider, const RigPiece& piece);

// The colliders that push piece at frame, each carried by its piece's
// pose, warp, the layer's move and what links carry it. Empty when
// nothing pushes piece.
std::vector<PushField> PushFieldsFor(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const LinkShifts& shifts,
    const QString& piece, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_COLLIDERS_H_
