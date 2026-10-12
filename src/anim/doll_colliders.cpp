#include "anim/doll_colliders.h"

#include <algorithm>
#include <cassert>

#include "anim/shot_grid.h"
#include "anim/warp.h"

namespace snapper {
namespace {

// The box around edges.
QRectF BoxOf(const std::vector<ColliderEdge>& edges) {
  assert(!edges.empty());
  assert(edges.size() <= kMaxColliderEdges);
  QRectF box(edges.front().a, edges.front().a);
  for (const ColliderEdge& edge : edges) {
    box |= QRectF(edge.a, edge.b).normalized();
  }
  return box;
}

// The grid points of piece at rest, in the drawing pixels of collider,
// with both pieces where the rig puts them as drawn.
std::vector<QPointF> RestInCollider(const Doll& doll, const RigPiece& piece,
                                    const RigPiece& collider,
                                    const std::map<QString, QTransform>& bind) {
  assert(piece.warp.IsOn());
  assert(!bind.empty());
  const ArtPiece* art = FindArt(doll, piece.name);
  const auto piece_bind = bind.find(piece.name);
  const auto collider_bind = bind.find(collider.name);
  bool is_invertible = false;
  const QTransform back =
      collider_bind != bind.end()
          ? collider_bind->second.inverted(&is_invertible)
          : QTransform();
  const bool is_placed =
      art != nullptr && piece_bind != bind.end() && is_invertible;
  if (!is_placed) {
    return {};
  }
  std::vector<QPointF> points = RestPoints(art->size, piece.warp);
  for (QPointF& point : points) {
    point = back.map(piece_bind->second.map(point));
  }
  return points;
}

}  // namespace

const Collider& ShownCollider(const Doll& doll, const PoseMap& poses,
                              const RigPiece& rig) {
  assert(!rig.name.isEmpty());
  assert(poses.size() <= static_cast<size_t>(kMaxDollPieces) * 4);
  const bool has_own = !rig.drawing_colliders.empty();
  if (!has_own) {
    return rig.collider;
  }
  const auto posed = poses.find(rig.name);
  const int drawing = posed != poses.end() ? posed->second.drawing : -1;
  return DrawingCollider(rig, ShownDrawingName(doll, rig.name, drawing));
}

bool IsPushing(const RigPiece& collider, const Collider& outline,
               const RigPiece& piece) {
  assert(outline.dots.size() <= static_cast<size_t>(kMaxColliderDots));
  assert(piece.warp.PointCount() <= kMaxWarpPoints);
  const auto& spared = outline.spared;
  const bool is_spared =
      std::find(spared.begin(), spared.end(), piece.name) != spared.end();
  const bool is_other = collider.name != piece.name;
  return outline.IsOn() && piece.warp.IsOn() && is_other && !is_spared;
}

std::vector<PushField> PushFieldsFor(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const LinkShifts& shifts,
    const QString& piece, Frame frame) {
  assert(frame.index() >= 0);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<PushField> fields;
  const RigPiece* pushed = FindRig(doll.rig, piece);
  const bool is_pushable = pushed != nullptr && pushed->warp.IsOn();
  if (!is_pushable) {
    return fields;
  }
  std::map<QString, QTransform> bind;
  const Collider& pushed_outline = ShownCollider(doll, poses, *pushed);
  for (const RigPiece& rig : doll.rig.pieces) {
    const Collider& outline = ShownCollider(doll, poses, rig);
    const bool is_pushing = IsPushing(rig, outline, *pushed);
    if (!is_pushing) {
      continue;
    }
    const bool is_first = bind.empty();
    if (is_first) {
      bind = PieceTransforms(doll, PoseMap());
    }
    const ArtPiece* art = FindArt(doll, rig.name);
    const auto grid = ShotGridOf(doll, layer, poses, placed, rig.name, frame,
                                 ShiftOf(shifts, rig.name, frame.index()));
    const std::vector<QPointF> rest =
        RestInCollider(doll, *pushed, rig, bind);
    const bool is_placed = art != nullptr && grid.has_value() &&
                           static_cast<int>(rest.size()) ==
                               pushed->warp.PointCount();
    if (!is_placed) {
      continue;
    }
    PushField field;
    field.pusher = rig.name;
    field.is_shared = IsPushing(*pushed, pushed_outline, rig);
    field.edges = ColliderEdges(
        outline,
        ShotDots(outline, rig.warp, art->size, grid->points,
                 grid->to_shot),
        grid->to_shot);
    const bool has_edges = !field.edges.empty();
    if (!has_edges) {
      continue;
    }
    field.box = BoxOf(field.edges);
    field.ghosts =
        ShotPoints(rest, rig.warp, art->size, grid->points, grid->to_shot);
    fields.push_back(std::move(field));
  }
  return fields;
}

}  // namespace snapper
