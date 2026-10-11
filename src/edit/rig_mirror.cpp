// RigManager's copying of one side of a doll onto the other.

#include <cassert>
#include <cmath>

#include "anim/mirror.h"
#include "anim/warp.h"
#include "base/text.h"
#include "anim/warp_shape.h"
#include "edit/group_mirror.h"
#include "edit/history_manager.h"
#include "edit/mesh_edits.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// from's mesh flipped left to right onto a drawing of to_size: dot k of
// one is the mirror of dot k of the other, and the triangles keep their
// turn. Nothing when the flipped dots do not make a mesh.
WarpGrid MirroredMesh(const WarpGrid& from, QSize from_size, QSize to_size) {
  assert(from.IsMesh());
  assert(to_size.width() >= 0 && to_size.height() >= 0);
  const WarpMesh& mesh = *from.mesh;
  const double across = from_size.width() > 0
                            ? to_size.width() /
                                  static_cast<double>(from_size.width())
                            : 1.0;
  const double down = from_size.height() > 0
                          ? to_size.height() /
                                static_cast<double>(from_size.height())
                          : 1.0;
  std::vector<QPointF> points;
  for (const QPointF& point : mesh.points) {
    points.emplace_back((from_size.width() - point.x()) * across,
                        point.y() * down);
  }
  std::vector<MeshTriangle> triangles;
  for (const MeshTriangle& t : mesh.triangles) {
    triangles.push_back({t[0], t[2], t[1]});
  }
  WarpGrid flipped;
  flipped.mesh = MakeWarpMesh(to_size, mesh.detail, std::move(points),
                              std::move(triangles));
  return flipped;
}

// to's warp made like from's (a mesh flipped from from's, or the same
// grid) with from's dot settings on the mirrored dots. A mesh that will
// not flip is made for to's own drawing.
void MirrorDotsOnto(const Doll& doll, const RigPiece& from, RigPiece* to) {
  assert(to != nullptr);
  assert(from.name != to->name);
  const bool is_mesh = from.warp.IsMesh();
  WarpGrid own = is_mesh ? MirroredMesh(from.warp,
                                        ArtSizeOf(doll, from.name),
                                        ArtSizeOf(doll, to->name))
                         : from.warp;
  const bool is_flipped = own.mesh != nullptr || !is_mesh;
  if (!is_flipped) {
    const Result<WarpGrid> shaped =
        ShapeMeshOf(doll, to->name, from.warp.mesh->detail);
    own = shaped.has_value() ? *shaped : from.warp;
  }
  to->warp = own;
  const std::vector<int> pairs =
      MirroredPoints(from.warp, ArtSizeOf(doll, from.name), to->warp,
                     ArtSizeOf(doll, to->name));
  const auto pair = [&pairs](int point) {
    return point >= 0 && static_cast<size_t>(point) < pairs.size()
               ? pairs[static_cast<size_t>(point)]
               : -1;
  };
  to->warp_reach.clear();
  const bool has_reach = static_cast<int>(from.warp_reach.size()) ==
                         from.warp.PointCount();
  if (has_reach) {
    to->warp_reach.assign(static_cast<size_t>(to->warp.PointCount()), 0.0);
  }
  for (int point = 0; has_reach && point < from.warp.PointCount(); ++point) {
    const int dot = pair(point);
    const bool is_paired = dot >= 0;
    if (is_paired) {
      to->warp_reach[static_cast<size_t>(dot)] =
          from.warp_reach[static_cast<size_t>(point)];
    }
  }
  to->drag_nodes.clear();
  to->point_motions.clear();
  for (const DragNode& node : from.drag_nodes) {
    DragNode copy = node;
    copy.point = pair(node.point);
    const bool is_paired = copy.point >= 0;
    if (is_paired) {
      to->drag_nodes.push_back(copy);
    }
  }
  for (const PointMotion& motion : from.point_motions) {
    const int dot = pair(motion.point);
    const bool is_paired = dot >= 0;
    if (is_paired) {
      to->point_motions.push_back(MirroredMotion(motion, dot));
    }
  }
}

// to's rig made the mirror of from's: the joint lands on the spot
// mirrored across the line halfway between the two drawings, so a doll
// drawn off-centre still lines up.
void MirrorPiece(const Doll& doll, const RigPiece& from, RigPiece* to) {
  assert(to != nullptr);
  assert(from.name != to->name);
  const ArtPiece* from_art = FindArt(doll, from.name);
  const ArtPiece* to_art = FindArt(doll, to->name);
  const bool has_art = from_art != nullptr && to_art != nullptr;
  if (!has_art) {
    return;
  }
  const double axis = (ArtCentreX(*from_art) + ArtCentreX(*to_art)) / 2.0;
  const QPointF joint = from_art->position + from.pivot;
  to->pivot = QPointF(2.0 * axis - joint.x(), joint.y()) - to_art->position;
  to->rest_rotation = -from.rest_rotation;
  to->keeps_shape = from.keeps_shape;
  MirrorDotsOnto(doll, from, to);
  const QString parent = MirrorName(from.parent);
  const bool has_mirrored_parent =
      !from.parent.isEmpty() && FindRig(doll.rig, parent) != nullptr;
  to->parent = has_mirrored_parent ? parent : from.parent;
}

// The other side's copy of chain, when it doesn't exist yet.
void MirrorChain(const Doll& doll, const IkChain& chain, Rig* rig) {
  assert(rig != nullptr);
  assert(!chain.lower.isEmpty());
  const IkChain* existing = nullptr;
  const QString lower = MirrorName(chain.lower);
  for (const IkChain& other : rig->chains) {
    existing = other.lower == lower ? &other : existing;
  }
  const ArtPiece* from = FindArt(doll, chain.lower);
  const ArtPiece* to = FindArt(doll, lower);
  const bool can_add = existing == nullptr && from != nullptr &&
                       to != nullptr && lower != chain.lower &&
                       rig->chains.size() < static_cast<size_t>(kMaxIkChains);
  if (!can_add) {
    return;
  }
  const double axis = (ArtCentreX(*from) + ArtCentreX(*to)) / 2.0;
  const QPointF tip = from->position + chain.tip;
  const QString name = MirrorName(chain.name);
  rig->chains.push_back(
      {name == chain.name ? Tr("%1 mirrored").arg(name) : name,
       MirrorName(chain.upper), lower,
       QPointF(2.0 * axis - tip.x(), tip.y()) - to->position,
       !chain.bends_clockwise});
}

}  // namespace

QString RigManager::WhyNoCopy(const QString& doll,
                              const QString& piece) const {
  assert(history_ != nullptr);
  assert(doll.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  const bool is_piece =
      found != nullptr && !piece.isEmpty() && FindRig(found->rig, piece);
  if (!is_piece) {
    return Tr("Pick a piece first.");
  }
  const QString partner = MirrorName(piece);
  const bool has_partner =
      partner != piece && FindRig(found->rig, partner) != nullptr;
  return has_partner
             ? QString()
             : Tr("%1 has no other-side twin; name pieces like arm_l and "
                  "arm_r.").arg(piece);
}

Result<void> RigManager::CopyToOtherSide(const QString& doll,
                                         const QString& piece,
                                         bool whole_side) {
  assert(history_ != nullptr);
  const QString why_not = WhyNoCopy(doll, piece);
  const bool can_copy = why_not.isEmpty();
  if (!can_copy) {
    return std::unexpected(Error{why_not});
  }
  const Doll& found = *FindDoll(history_->current(), doll);
  const int side = SideOf(piece);
  assert(side != 0);
  Doll next = found;
  std::vector<QString> copied;
  for (const RigPiece& from : found.rig.pieces) {
    const bool is_copied =
        whole_side ? SideOf(from.name) == side : from.name == piece;
    RigPiece* to =
        is_copied ? FindRig(&next.rig, MirrorName(from.name)) : nullptr;
    const bool has_twin = to != nullptr && to->name != from.name;
    if (has_twin) {
      MirrorPiece(found, from, to);
      copied.push_back(from.name);
    }
  }
  for (const IkChain& chain : found.rig.chains) {
    const bool is_copied = whole_side && SideOf(chain.lower) == side;
    if (is_copied) {
      MirrorChain(found, chain, &next.rig);
    }
  }
  for (const RigPiece& piece_copy : next.rig.pieces) {
    const bool is_tree =
        piece_copy.parent.isEmpty() ||
        CanParent(next.rig, piece_copy.name, piece_copy.parent);
    if (!is_tree) {
      return std::unexpected(Error{
          Tr("Copying would make %1 hang from itself; check the parents "
             "on this side first.").arg(piece_copy.name)});
    }
  }
  Project project = WithTwinGroups(history_->current(), doll, found,
                                   std::move(next), copied);
  return history_->Apply(whole_side ? Tr("Copy side to the other")
                                    : Tr("Copy %1 to the other side")
                                          .arg(piece),
                         std::move(project));
}

}  // namespace snapper
