#include "anim/doll_collide.h"

#include <QTransform>

#include <algorithm>
#include <cassert>
#include <vector>

#include "anim/doll_colliders.h"
#include "anim/grid_push.h"
#include "anim/shot_grid.h"

namespace snapper {
namespace {

// The rig's pieces back to front (by draw order, then rig order).
std::vector<const RigPiece*> BackToFront(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(kMaxDollPieces > 0);
  std::vector<const RigPiece*> pieces;
  for (const RigPiece& rig : doll.rig.pieces) {
    pieces.push_back(&rig);
  }
  std::stable_sort(pieces.begin(), pieces.end(),
                   [](const RigPiece* a, const RigPiece* b) {
                     return a->order < b->order;
                   });
  return pieces;
}

// Adds the move of each grid point from before to after (shot space)
// to the warp of pose, in the drawing pixels to_shot maps from.
void AddPush(const QTransform& to_shot, const std::vector<QPointF>& before,
             const std::vector<QPointF>& after, PiecePose* pose) {
  assert(pose != nullptr);
  assert(before.size() == after.size());
  bool is_invertible = false;
  const QTransform back = to_shot.inverted(&is_invertible);
  if (!is_invertible) {
    return;
  }
  const bool is_fitting = pose->warp.size() == before.size();
  if (!is_fitting) {
    pose->warp.assign(before.size(), QPointF());
  }
  for (size_t i = 0; i < before.size(); ++i) {
    pose->warp[i] += back.map(after[i]) - back.map(before[i]);
  }
}

}  // namespace

void CollidePoses(const Doll& doll, const Layer& layer, Frame frame,
                  const LinkShifts& shifts, PoseMap* poses) {
  assert(poses != nullptr);
  assert(frame.index() >= 0);
  const bool has_collider =
      std::any_of(doll.rig.pieces.begin(), doll.rig.pieces.end(),
                  [](const RigPiece& rig) { return rig.collider.IsOn(); });
  if (!has_collider) {
    return;
  }
  // Warps never move a piece's frame, so one placing serves every piece.
  const auto placed = PieceTransforms(doll, *poses);
  for (const RigPiece* rig : BackToFront(doll)) {
    const std::vector<PushField> fields = PushFieldsFor(
        doll, layer, *poses, placed, shifts, rig->name, frame);
    const bool is_pushed = !fields.empty();
    if (!is_pushed) {
      continue;
    }
    const auto grid =
        ShotGridOf(doll, layer, *poses, placed, rig->name, frame,
                   ShiftOf(shifts, rig->name, frame.index()));
    const bool is_placed = grid.has_value() &&
                           static_cast<int>(grid->points.size()) ==
                               rig->warp.PointCount();
    if (!is_placed) {
      continue;
    }
    std::vector<QPointF> points = grid->points;
    PushGrid(rig->warp, ReachesOf(*rig), fields, &points);
    const bool is_moved = points != grid->points;
    if (is_moved) {
      AddPush(grid->to_shot, grid->points, points, &(*poses)[rig->name]);
    }
  }
}

}  // namespace snapper
