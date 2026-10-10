// RigManager's shape meshes: a warp that hugs the drawing, made from
// the piece's drawings, at a detail.

#include <cassert>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/mesh_edits.h"
#include "edit/rig_manager.h"

namespace snapper {

Result<CarryCount> RigManager::SetShapeMeshAll(
    const QString& doll, const std::vector<QString>& pieces,
    MeshDetail detail) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool has_pieces = !pieces.empty();
  if (!has_pieces) {
    return std::unexpected(Error{Tr("Pick a piece first.")});
  }
  Project next = history_->current();
  CarryCount count;
  bool is_changed = false;
  for (const QString& piece : pieces) {
    const Doll* found = FindDoll(next, doll);
    const RigPiece* rig = found != nullptr ? FindRig(found->rig, piece)
                                           : nullptr;
    const bool is_piece = rig != nullptr;
    if (!is_piece) {
      return std::unexpected(
          Error{Tr("%1 has no piece called %2.").arg(doll, piece)});
    }
    const Result<WarpGrid> fresh = ShapeMeshOf(*found, piece, detail);
    const bool is_made = fresh.has_value();
    if (!is_made) {
      return std::unexpected(fresh.error());
    }
    const bool is_same = *fresh == rig->warp;
    if (is_same) {
      continue;
    }
    Result<Project> changed = WithWarp(next, doll, piece, *fresh, &count);
    const bool is_done = changed.has_value();
    if (!is_done) {
      return std::unexpected(changed.error());
    }
    next = std::move(*changed);
    is_changed = true;
  }
  if (!is_changed) {
    return count;
  }
  const QString label = pieces.size() == 1
                            ? Tr("Shape mesh of %1").arg(pieces[0])
                            : Tr("Shape mesh of %1 pieces").arg(pieces.size());
  const Result<void> applied = history_->Apply(label, std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return count;
}

QString RigManager::MeshDoneText(const CarryCount& count) {
  assert(count.carried >= 0);
  assert(count.dropped >= 0);
  const bool has_settings = count.carried + count.dropped > 0;
  if (!has_settings) {
    return Tr("Shape mesh made.");
  }
  const QString carried =
      count.carried == 1
          ? Tr("Shape mesh made: 1 dot's settings carried over")
          : Tr("Shape mesh made: %1 dots' settings carried over")
                .arg(count.carried);
  const bool has_dropped = count.dropped > 0;
  return has_dropped
             ? Tr("%1; %2 found no dot near enough and were let go.")
                   .arg(carried)
                   .arg(count.dropped)
             : carried + QStringLiteral(".");
}

const RigPiece* RigManager::RigOf(const QString& doll,
                                  const QString& piece) const {
  assert(history_ != nullptr);
  assert(doll.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  return found != nullptr && !piece.isEmpty() ? FindRig(found->rig, piece)
                                              : nullptr;
}

QString RigManager::WhyNoDetail(const QString& doll,
                                const QString& piece) const {
  assert(doll.size() < 100000);
  const RigPiece* rig = RigOf(doll, piece);
  assert(history_ != nullptr);
  const bool is_piece = rig != nullptr;
  if (!is_piece) {
    return Tr("Pick a piece first.");
  }
  const bool is_on = rig->warp.IsOn();
  if (!is_on) {
    return Tr("Tick \"Bend\" first.");
  }
  const bool is_mesh = rig->warp.IsMesh();
  return is_mesh ? QString()
                 : Tr("This piece bends with an old grid; convert it to a "
                      "shape mesh first.");
}

QString RigManager::WhyNoConvert(const QString& doll,
                                 const QString& piece) const {
  assert(doll.size() < 100000);
  const RigPiece* rig = RigOf(doll, piece);
  assert(history_ != nullptr);
  const bool is_piece = rig != nullptr;
  if (!is_piece) {
    return Tr("Pick a piece first.");
  }
  const bool is_grid = rig->warp.IsOn() && !rig->warp.IsMesh();
  return is_grid ? QString()
                 : Tr("Only a piece that bends with an old grid needs "
                      "converting.");
}

}  // namespace snapper
