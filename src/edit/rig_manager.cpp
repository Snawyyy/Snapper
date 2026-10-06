#include "edit/rig_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"

namespace snapper {

RigManager::RigManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<void> RigManager::SetParent(const QString& doll, const QString& piece,
                                   const QString& parent) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Parent %1").arg(piece),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const bool is_allowed = CanParent(*rig, piece, parent);
        if (!is_allowed) {
          return std::unexpected(Error{
              Tr("%1 can't hang from %2: that would make a loop, or one "
                 "of them is missing.").arg(piece, parent)});
        }
        FindRig(rig, piece)->parent = parent;
        return {};
      }));
}

Result<void> RigManager::SetPivot(const QString& doll, const QString& piece,
                                  QPointF pivot) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const bool is_finite = std::isfinite(pivot.x()) && std::isfinite(pivot.y());
  if (!is_finite) {
    return std::unexpected(Error{Tr("That pivot is off the map.")});
  }
  return history_->Apply(
      Tr("Move pivot of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [pivot](RigPiece* rig) -> Result<void> {
                  rig->pivot = pivot;
                  return {};
                }));
}

Result<void> RigManager::SetOrder(const QString& doll, const QString& piece,
                                  int order) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  return history_->Apply(
      Tr("Restack %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [order](RigPiece* rig) -> Result<void> {
                  rig->order = order;
                  return {};
                }));
}

Result<void> RigManager::SetRestRotation(const QString& doll,
                                         const QString& piece,
                                         double degrees) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const bool is_finite = std::isfinite(degrees);
  if (!is_finite) {
    return std::unexpected(Error{Tr("That angle is off the map.")});
  }
  return history_->Apply(
      Tr("Rest turn of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [degrees](RigPiece* rig) -> Result<void> {
                  rig->rest_rotation = degrees;
                  return {};
                }));
}

Result<void> RigManager::SetDefaultDrawing(const QString& doll,
                                           const QString& piece,
                                           int drawing) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const Doll* found = FindDoll(history_->current(), doll);
  const ArtPiece* art = found != nullptr ? FindArt(*found, piece) : nullptr;
  const int count = art != nullptr ? static_cast<int>(art->drawings.size())
                                   : 0;
  const bool is_in_range = drawing >= -1 && drawing < count;
  if (!is_in_range) {
    return std::unexpected(
        Error{Tr("%1 has no drawing %2.").arg(piece).arg(drawing + 1)});
  }
  return history_->Apply(
      Tr("Default drawing of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [drawing](RigPiece* rig) -> Result<void> {
                  rig->default_drawing = drawing;
                  return {};
                }));
}

Result<void> RigManager::SetWarpGrid(const QString& doll,
                                     const QString& piece, WarpGrid grid) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const bool is_off = grid.columns == 0 && grid.rows == 0;
  const bool is_valid =
      is_off || (grid.columns >= 1 && grid.rows >= 1 &&
                 grid.columns <= kMaxWarpCells && grid.rows <= kMaxWarpCells);
  if (!is_valid) {
    return std::unexpected(Error{
        Tr("A warp grid is 1 to %1 cells each way.").arg(kMaxWarpCells)});
  }
  auto next = WithPiece(history_->current(), doll, piece,
                        [grid](RigPiece* rig) -> Result<void> {
                          rig->warp = grid;
                          rig->warp_reach.clear();
                          return {};
                        });
  const bool is_changed = next.has_value();
  if (is_changed) {
    *next = WithoutWarpKeys(std::move(*next), doll, piece);
  }
  return history_->Apply(Tr("Warp grid of %1").arg(piece), std::move(next));
}

Result<void> RigManager::SetWarpReach(const QString& doll,
                                      const QString& piece, int point,
                                      double reach) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const bool is_number = std::isfinite(reach);
  if (!is_number) {
    return std::unexpected(Error{Tr("That reach is off the map.")});
  }
  const double cells = std::clamp(reach, 0.0, double(kMaxWarpCells));
  return history_->Apply(
      Tr("Rubber reach of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [point, cells](RigPiece* rig) -> Result<void> {
                  const int count = rig->warp.PointCount();
                  const bool is_point = point >= 0 && point < count;
                  if (!is_point) {
                    return std::unexpected(Error{
                        Tr("Give the piece a warp grid in the rig first.")});
                  }
                  rig->warp_reach.resize(static_cast<size_t>(count), 0.0);
                  rig->warp_reach[static_cast<size_t>(point)] = cells;
                  return {};
                }));
}

Result<void> RigManager::AddChain(const QString& doll, const IkChain& chain) {
  assert(history_ != nullptr);
  assert(kMaxIkChains > 0);
  return history_->Apply(
      Tr("Add IK %1").arg(chain.name),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const RigPiece* lower =
            chain.lower.isEmpty() ? nullptr : FindRig(*rig, chain.lower);
        const bool is_linked = !chain.upper.isEmpty() && lower != nullptr &&
                               lower->parent == chain.upper;
        const bool is_named =
            !chain.name.trimmed().isEmpty() &&
            FindChain(*rig, chain.name) == nullptr;
        const bool has_room =
            rig->chains.size() < static_cast<size_t>(kMaxIkChains);
        const bool is_valid = is_linked && is_named && has_room;
        if (!is_valid) {
          return std::unexpected(Error{
              Tr("An IK chain needs a name not used yet, and its lower "
                 "piece must hang from its upper piece.")});
        }
        rig->chains.push_back(chain);
        return {};
      }));
}

Result<void> RigManager::RemoveChain(const QString& doll,
                                     const QString& chain) {
  assert(history_ != nullptr);
  assert(!chain.isEmpty());
  return history_->Apply(
      Tr("Remove IK %1").arg(chain),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const auto removed =
            std::erase_if(rig->chains, [&chain](const IkChain& c) {
              return c.name == chain;
            });
        const bool is_removed = removed > 0;
        if (!is_removed) {
          return std::unexpected(NoChain(doll, chain));
        }
        return {};
      }));
}

Result<void> RigManager::SetChainBend(const QString& doll,
                                      const QString& chain,
                                      bool bends_clockwise) {
  assert(history_ != nullptr);
  assert(!chain.isEmpty());
  return history_->Apply(
      Tr("Flip IK bend of %1").arg(chain),
      WithChain(history_->current(), doll, chain,
                [bends_clockwise](IkChain* found) -> Result<void> {
                  found->bends_clockwise = bends_clockwise;
                  return {};
                }));
}

Result<void> RigManager::SetChainTip(const QString& doll, const QString& chain,
                                     QPointF tip) {
  assert(history_ != nullptr);
  assert(!chain.isEmpty());
  const bool is_finite = std::isfinite(tip.x()) && std::isfinite(tip.y());
  if (!is_finite) {
    return std::unexpected(Error{Tr("That point is off the map.")});
  }
  return history_->Apply(
      Tr("Move IK tip of %1").arg(chain),
      WithChain(history_->current(), doll, chain,
                [tip](IkChain* found) -> Result<void> {
                  found->tip = tip;
                  return {};
                }));
}

}  // namespace snapper
