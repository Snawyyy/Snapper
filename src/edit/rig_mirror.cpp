// RigManager's copying of one side of a doll onto the other.

#include <cassert>

#include "anim/mirror.h"
#include "anim/warp.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// The middle of a piece's drawing in doll space.
double CentreX(const ArtPiece& art) {
  assert(art.size.width() >= 0);
  assert(std::isfinite(art.position.x()));
  return art.position.x() + art.size.width() / 2.0;
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
  const double axis = (CentreX(*from_art) + CentreX(*to_art)) / 2.0;
  const QPointF joint = from_art->position + from.pivot;
  to->pivot = QPointF(2.0 * axis - joint.x(), joint.y()) - to_art->position;
  to->rest_rotation = -from.rest_rotation;
  to->warp = from.warp;
  to->keeps_shape = from.keeps_shape;
  to->warp_reach = from.warp_reach;
  const bool has_reach = static_cast<int>(from.warp_reach.size()) ==
                         from.warp.PointCount();
  for (int point = 0; has_reach && point < from.warp.PointCount(); ++point) {
    to->warp_reach[static_cast<size_t>(MirroredPoint(from.warp, point))] =
        from.warp_reach[static_cast<size_t>(point)];
  }
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
  const double axis = (CentreX(*from) + CentreX(*to)) / 2.0;
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
  for (const RigPiece& from : found.rig.pieces) {
    const bool is_copied =
        whole_side ? SideOf(from.name) == side : from.name == piece;
    RigPiece* to =
        is_copied ? FindRig(&next.rig, MirrorName(from.name)) : nullptr;
    const bool has_twin = to != nullptr && to->name != from.name;
    if (has_twin) {
      MirrorPiece(found, from, to);
    }
  }
  for (const IkChain& chain : found.rig.chains) {
    const bool is_copied = whole_side && SideOf(chain.lower) == side;
    if (is_copied) {
      MirrorChain(found, chain, &next.rig);
    }
  }
  for (const RigPiece& copied : next.rig.pieces) {
    const bool is_tree = copied.parent.isEmpty() ||
                         CanParent(next.rig, copied.name, copied.parent);
    if (!is_tree) {
      return std::unexpected(Error{
          Tr("Copying would make %1 hang from itself; check the parents "
             "on this side first.").arg(copied.name)});
    }
  }
  Project project = history_->current();
  project.dolls[doll] = std::make_shared<const Doll>(std::move(next));
  return history_->Apply(whole_side ? Tr("Copy side to the other")
                                    : Tr("Copy %1 to the other side")
                                          .arg(piece),
                         std::move(project));
}

}  // namespace snapper
