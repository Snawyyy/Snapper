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
  // The piece whose outline this is.
  QString pusher;
  // True when the pushed piece's own collider pushes that piece back
  // (two squashy pieces meeting): the two share the move, so neither
  // keeps its shape while the other gives way for both.
  bool is_shared = false;
  std::vector<ColliderEdge> edges;
  QRectF box;
  std::vector<QPointF> ghosts;
};

// The collider of the drawing rig shows in poses (see DrawingCollider).
const Collider& ShownCollider(const Doll& doll, const PoseMap& poses,
                              const RigPiece& rig);

// Whether collider, with outline (the collider of the drawing it
// shows), pushes piece: it is another piece of the doll, has an outline,
// and does not spare it, whichever is drawn in front, so two hands with
// colliders push each other apart. Only a piece with a warp grid can be
// pushed.
bool IsPushing(const RigPiece& collider, const Collider& outline,
               const RigPiece& piece);

// The colliders that push piece at frame, each the collider of the
// drawing its piece shows in poses and carried by its piece's pose,
// warp, the layer's move and what links carry it. Empty when nothing
// pushes piece.
std::vector<PushField> PushFieldsFor(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const LinkShifts& shifts,
    const QString& piece, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_COLLIDERS_H_
