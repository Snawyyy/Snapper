// RigManager's changes to many pieces at once.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// change(RigPiece*) on each of pieces in rig; any missing is refused.
template <typename Change>
Result<void> EachPiece(Rig* rig, const QString& doll,
                       const std::vector<QString>& pieces, Change change) {
  assert(rig != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool is_empty = pieces.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick one or more pieces first.")});
  }
  for (const QString& name : pieces) {
    RigPiece* piece = FindRig(rig, name);
    const bool is_found = piece != nullptr;
    if (!is_found) {
      return std::unexpected(NoPiece(doll, name));
    }
    const Result<void> changed = change(piece);
    if (!changed) {
      return changed;
    }
  }
  return {};
}

// change(WarpMotion*) on each of pieces; a piece without a warp grid
// has nothing to move, so it is refused.
template <typename Change>
Result<Project> EachMotion(const Project& project, const QString& doll,
                           const std::vector<QString>& pieces,
                           Change change) {
  assert(!doll.isEmpty());
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return WithRig(project, doll, [&](Rig* rig) {
    return EachPiece(rig, doll, pieces, [&](RigPiece* piece) {
      const bool has_grid = piece->warp.IsOn();
      if (!has_grid) {
        return Result<void>(std::unexpected(Error{
            Tr("Tick \"Bend with a grid\" for %1 first.").arg(piece->name)}));
      }
      change(&piece->warp_motion);
      // MotionPushes divides by the cycle, so no change may leave it out
      // of range.
      assert(piece->warp_motion.cycle >= kMinWarpCycle &&
             piece->warp_motion.cycle <= kMaxWarpCycle);
      return Result<void>();
    });
  });
}

}  // namespace

Result<void> RigManager::SetMotionKindAll(const QString& doll,
                                          const std::vector<QString>& pieces,
                                          WarpMotionKind kind) {
  assert(history_ != nullptr);
  assert(static_cast<int>(kind) < kWarpMotionKindCount);
  return history_->Apply(
      Tr("Warp animation"),
      EachMotion(history_->current(), doll, pieces,
                 [kind](WarpMotion* motion) { motion->kind = kind; }));
}

Result<void> RigManager::SetMotionEdgeAll(const QString& doll,
                                          const std::vector<QString>& pieces,
                                          WarpEdge edge) {
  assert(history_ != nullptr);
  assert(static_cast<int>(edge) < kWarpEdgeCount);
  return history_->Apply(
      Tr("Animation edge"),
      EachMotion(history_->current(), doll, pieces,
                 [edge](WarpMotion* motion) { motion->edge = edge; }));
}

Result<void> RigManager::ShiftMotionAll(const QString& doll,
                                        const std::vector<QString>& pieces,
                                        double size, int cycle) {
  assert(history_ != nullptr);
  assert(std::abs(cycle) <= kMaxWarpCycle * 2);
  const bool is_number = std::isfinite(size);
  if (!is_number) {
    return std::unexpected(Error{Tr("That size is off the map.")});
  }
  return history_->Apply(
      size != 0.0 ? Tr("Animation size") : Tr("Animation speed"),
      EachMotion(history_->current(), doll, pieces,
                 [size, cycle](WarpMotion* motion) {
                   motion->size = std::clamp(motion->size + size, 0.0,
                                             kMaxWarpMotionSize);
                   motion->cycle = std::clamp(motion->cycle + cycle,
                                              kMinWarpCycle, kMaxWarpCycle);
                 }));
}

Result<void> RigManager::ShiftRestAll(const QString& doll,
                                      const std::vector<QString>& pieces,
                                      double delta) {
  assert(history_ != nullptr);
  assert(std::isfinite(delta));
  return history_->Apply(
      Tr("Rest turn"),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        return EachPiece(rig, doll, pieces, [delta](RigPiece* piece) {
          piece->rest_rotation += delta;
          return Result<void>();
        });
      }));
}

Result<void> RigManager::ShiftOrderAll(const QString& doll,
                                       const std::vector<QString>& pieces,
                                       int delta) {
  assert(history_ != nullptr);
  assert(std::abs(delta) < 100000);
  return history_->Apply(
      Tr("Restack"),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        return EachPiece(rig, doll, pieces, [delta](RigPiece* piece) {
          piece->order += delta;
          return Result<void>();
        });
      }));
}

Result<void> RigManager::SetParentAll(const QString& doll,
                                      const std::vector<QString>& pieces,
                                      const QString& parent) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return history_->Apply(
      pieces.size() == 1 ? Tr("Parent %1").arg(pieces.front())
                         : Tr("Parent pieces"),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        return EachPiece(rig, doll, pieces, [&](RigPiece* piece) {
          // Checked one at a time against the rig as it grows, so a
          // loop through two picked pieces is caught too.
          const bool can_hang = CanParent(*rig, piece->name, parent);
          if (!can_hang) {
            return Result<void>(std::unexpected(Error{
                Tr("%1 can't hang from %2: that would make a loop.")
                    .arg(piece->name, parent)}));
          }
          piece->parent = parent;
          return Result<void>();
        });
      }));
}

Result<void> RigManager::SetWarpAll(const QString& doll,
                                    const std::vector<QString>& pieces,
                                    WarpGrid grid) {
  assert(history_ != nullptr);
  assert(grid.columns >= 0 && grid.rows >= 0);
  auto next = WithRig(history_->current(), doll, [&](Rig* rig) {
    return EachPiece(rig, doll, pieces, [grid](RigPiece* piece) {
      piece->warp = grid;
      piece->warp_reach.clear();
      piece->drag_nodes.clear();
      return Result<void>();
    });
  });
  for (const QString& piece : pieces) {
    const bool is_changed = next.has_value();
    if (is_changed) {
      *next = WithoutWarpKeys(std::move(*next), doll, piece);
    }
  }
  return history_->Apply(Tr("Warp grid"), std::move(next));
}

Result<void> RigManager::SetKeepShapeAll(const QString& doll,
                                         const std::vector<QString>& pieces,
                                         bool keeps_shape) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return history_->Apply(
      keeps_shape ? Tr("Keep shape") : Tr("Let shape squash"),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        return EachPiece(rig, doll, pieces, [keeps_shape](RigPiece* piece) {
          piece->keeps_shape = keeps_shape;
          return Result<void>();
        });
      }));
}

Result<void> RigManager::MovePivots(
    const QString& doll, const std::map<QString, QPointF>& offsets) {
  assert(history_ != nullptr);
  assert(offsets.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<QString> pieces;
  for (const auto& [name, offset] : offsets) {
    pieces.push_back(name);
  }
  return history_->Apply(
      Tr("Move joints"),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        return EachPiece(rig, doll, pieces, [&offsets](RigPiece* piece) {
          piece->pivot += offsets.at(piece->name);
          return Result<void>();
        });
      }));
}

Result<void> RigManager::CopyAllToOtherSide(
    const QString& doll, const std::vector<QString>& pieces) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  EditScope batch(history_, Tr("Copy to the other side"));
  int copied = 0;
  for (const QString& piece : pieces) {
    const bool has_twin = WhyNoCopy(doll, piece).isEmpty();
    if (has_twin) {
      const auto done = CopyToOtherSide(doll, piece, false);
      if (!done) {
        batch.Cancel();
        return done;
      }
      ++copied;
    }
  }
  const bool is_none = copied == 0;
  if (is_none) {
    batch.Cancel();
    return std::unexpected(
        Error{Tr("None of those pieces has an other-side twin.")});
  }
  return {};
}

}  // namespace snapper
