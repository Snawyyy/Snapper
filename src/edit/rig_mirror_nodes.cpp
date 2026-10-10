// RigManager's mirroring of drag and physics nodes and colliders onto
// the other side's piece.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>

#include "anim/mirror.h"
#include "anim/warp.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// from's collider on to's drawing, mirrored across the line halfway
// between the two drawings. The ring runs the other way round the
// mirrored shape, so its slots count down and the dots keep their
// neighbours.
Collider MirroredCollider(const Doll& doll, const RigPiece& from,
                          const RigPiece& to) {
  assert(from.name != to.name);
  assert(from.collider.ring_size <= kMaxColliderDots);
  const ArtPiece* from_art = FindArt(doll, from.name);
  const ArtPiece* to_art = FindArt(doll, to.name);
  const bool has_art = from_art != nullptr && to_art != nullptr;
  const bool is_collider = from.collider.IsOn();
  const bool is_usable = has_art && is_collider;
  if (!is_usable) {
    return Collider();
  }
  const double axis = (ArtCentreX(*from_art) + ArtCentreX(*to_art)) / 2.0;
  Collider mirrored;
  mirrored.ring_size = from.collider.ring_size;
  for (auto dot = from.collider.dots.rbegin();
       dot != from.collider.dots.rend(); ++dot) {
    const QPointF at = from_art->position + dot->at;
    mirrored.dots.push_back(
        {QPointF(2.0 * axis - at.x(), at.y()) - to_art->position,
         from.collider.ring_size - 1 - dot->slot});
  }
  for (const QString& name : from.collider.spared) {
    const QString twin = MirrorName(name);
    const bool has_twin = FindRig(doll.rig, twin) != nullptr;
    mirrored.spared.push_back(has_twin ? twin : name);
  }
  return mirrored;
}

// The other side's piece of piece, or nothing.
const RigPiece* PartnerOf(const Doll& doll, const QString& piece) {
  assert(piece.size() < 100000);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const QString partner = MirrorName(piece);
  const bool is_twin = partner != piece;
  return is_twin ? FindRig(doll.rig, partner) : nullptr;
}

}  // namespace

QString RigManager::WhyNoMirror(const QString& doll,
                                const QString& piece) const {
  assert(history_ != nullptr);
  assert(doll.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  const RigPiece* rig = found != nullptr && !piece.isEmpty()
                            ? FindRig(found->rig, piece)
                            : nullptr;
  const bool is_piece = rig != nullptr;
  if (!is_piece) {
    return Tr("Pick a piece first.");
  }
  const RigPiece* partner = PartnerOf(*found, piece);
  const bool has_partner = partner != nullptr;
  if (!has_partner) {
    return Tr("%1 has no other-side twin; name pieces like arm_l and "
              "arm_r.").arg(piece);
  }
  const bool is_same_grid = partner->warp == rig->warp;
  return is_same_grid ? QString()
                      : Tr("%1 and %2 need the same warp grid first.")
                            .arg(piece, partner->name);
}

Result<void> RigManager::MirrorNodes(const QString& doll,
                                     const QString& piece) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const QString why_not = WhyNoMirror(doll, piece);
  const bool can_mirror = why_not.isEmpty();
  if (!can_mirror) {
    return std::unexpected(Error{why_not});
  }
  const Doll& found = *FindDoll(history_->current(), doll);
  const RigPiece& from = *FindRig(found.rig, piece);
  const QString partner = MirrorName(piece);
  return history_->Apply(
      Tr("Mirror %1 to %2").arg(piece, partner),
      WithPiece(history_->current(), doll, partner,
                [&](RigPiece* to) -> Result<void> {
                  to->drag_nodes = from.drag_nodes;
                  for (DragNode& node : to->drag_nodes) {
                    node.point = MirroredPoint(from.warp, node.point);
                  }
                  to->point_motions.clear();
                  for (const PointMotion& motion : from.point_motions) {
                    to->point_motions.push_back(
                        MirroredMotion(motion, from.warp));
                  }
                  to->collider = MirroredCollider(found, from, *to);
                  return {};
                }));
}

Result<void> RigManager::MirrorDots(const std::vector<DotRef>& dots) {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return std::unexpected(Error{Tr("Pick one or more warp dots first.")});
  }
  Result<Project> next = history_->current();
  for (const DotRef& dot : dots) {
    const QString why_not = WhyNoMirror(dot.doll, dot.piece);
    const bool can_mirror = why_not.isEmpty();
    if (!can_mirror) {
      return std::unexpected(Error{why_not});
    }
    const Doll& found = *FindDoll(*next, dot.doll);
    const RigPiece& from = *FindRig(found.rig, dot.piece);
    const DragNode* node = nullptr;
    for (const DragNode& n : from.drag_nodes) {
      node = n.point == dot.point ? &n : node;
    }
    const PointMotion* moving = FindMotion(from, dot.point);
    const int point = MirroredPoint(from.warp, dot.point);
    next = WithPiece(
        *next, dot.doll, MirrorName(dot.piece),
        [&](RigPiece* to) -> Result<void> {
          std::erase_if(to->drag_nodes, [point](const DragNode& n) {
            return n.point == point;
          });
          const bool is_node = node != nullptr;
          if (is_node) {
            DragNode copy = *node;
            copy.point = point;
            to->drag_nodes.push_back(copy);
          }
          std::erase_if(to->point_motions, [point](const PointMotion& m) {
            return m.point == point;
          });
          const bool is_moving = moving != nullptr;
          if (is_moving) {
            to->point_motions.push_back(MirroredMotion(*moving, from.warp));
          }
          return {};
        });
    const bool is_refused = !next;
    if (is_refused) {
      return std::unexpected(next.error());
    }
  }
  return history_->Apply(
      dots.size() == 1 ? Tr("Mirror 1 dot to the other side")
                       : Tr("Mirror %1 dots to the other side")
                             .arg(dots.size()),
      std::move(next));
}

}  // namespace snapper
