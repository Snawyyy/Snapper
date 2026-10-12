// RigManager: making one layer's warp dot settings the doll's default.

#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <variant>

#include "edit/rig_edits.h"
#include "edit/rig_manager.h"
#include "base/text.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

// The layers of every shot that show doll, but layer.
std::set<LayerId> OtherLayersOf(const Project& project, const QString& doll,
                                LayerId layer) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(!doll.isEmpty());
  std::set<LayerId> others;
  for (const auto& shot : project.shots) {
    for (const Layer& item : shot->layers) {
      const auto* posed = std::get_if<DollLayer>(&item.content);
      const bool is_other =
          posed != nullptr && posed->doll == doll && item.id != layer;
      if (is_other) {
        others.insert(item.id);
      }
    }
  }
  return others;
}

}  // namespace

bool RigManager::HasOwnDots(const QString& doll, LayerId layer) const {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const Doll* found = FindDoll(history_->current(), doll);
  const bool has_doll = found != nullptr;
  if (!has_doll) {
    return false;
  }
  return std::any_of(found->rig.pieces.begin(), found->rig.pieces.end(),
                     [layer](const RigPiece& piece) {
                       return piece.layer_dots.contains(layer);
                     });
}

Result<void> RigManager::SetDotsDefault(const QString& doll, LayerId layer) {
  assert(history_ != nullptr);
  assert(!doll.isEmpty());
  const Project& project = history_->current();
  const Doll* found = FindDoll(project, doll);
  const bool has_doll = found != nullptr && layer.IsValid();
  if (!has_doll) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  const bool has_own = HasOwnDots(doll, layer);
  if (!has_own) {
    return std::unexpected(Error{
        Tr("This doll's warp settings here already are the default.")});
  }
  const std::set<LayerId> others = OtherLayersOf(project, doll, layer);
  Doll changed = *found;
  for (RigPiece& piece : changed.rig.pieces) {
    const auto own = piece.layer_dots.find(layer);
    const bool is_own = own != piece.layer_dots.end();
    if (!is_own) {
      continue;
    }
    // Every other layer keeps the look it has now.
    const PieceDots before{piece.warp_reach, piece.drag_nodes,
                           piece.point_motions};
    for (const LayerId other : others) {
      piece.layer_dots.try_emplace(other, before);
    }
    ShowDots(own->second, &piece);
    piece.layer_dots.erase(layer);
  }
  Project next = project;
  next.dolls[doll] = std::make_shared<const Doll>(std::move(changed));
  return history_->Apply(Tr("Set default warp settings of %1").arg(doll),
                         std::move(next));
}

}  // namespace snapper
