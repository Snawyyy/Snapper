#include "anim/doll_colliders.h"

#include <algorithm>
#include <cassert>

#include "anim/shot_grid.h"

namespace snapper {

std::vector<ColliderEdge> CollidersFor(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const LinkShifts& shifts,
    const QString& piece, Frame frame) {
  assert(frame.index() >= 0);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<ColliderEdge> edges;
  const RigPiece* cloth = FindRig(doll.rig, piece);
  const bool is_cloth_piece = cloth != nullptr;
  if (!is_cloth_piece) {
    return edges;
  }
  for (const RigPiece& rig : doll.rig.pieces) {
    const Collider& collider = rig.collider;
    const bool is_behind = rig.order < cloth->order;
    const bool is_spared =
        std::find(collider.spared.begin(), collider.spared.end(), piece) !=
        collider.spared.end();
    const bool is_pushing = collider.IsOn() && is_behind && !is_spared;
    if (!is_pushing) {
      continue;
    }
    const ArtPiece* art = FindArt(doll, rig.name);
    const auto grid = ShotGridOf(doll, layer, poses, placed, rig.name, frame,
                                 ShiftOf(shifts, rig.name, frame.index()));
    const bool is_placed = art != nullptr && grid.has_value();
    if (!is_placed) {
      continue;
    }
    const std::vector<QPointF> dots =
        ShotDots(collider, rig.warp, art->size, grid->points, grid->to_shot);
    const std::vector<ColliderEdge> made =
        ColliderEdges(collider, dots, grid->to_shot);
    edges.insert(edges.end(), made.begin(), made.end());
  }
  return edges;
}

}  // namespace snapper
