#include "ui/picked_dot.h"

#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"

namespace snapper {

std::optional<PickedDot> PickedDotOf(const Managers& managers) {
  assert(managers.selection != nullptr);
  assert(managers.history != nullptr);
  const auto dot = managers.selection->dot();
  const Project& project = managers.history->current();
  const ShotId shot = managers.selection->shot();
  const DollLayer* posed =
      dot ? PosedLayerOf(project, shot, dot->layer) : nullptr;
  const Doll* doll = dot ? DollOfLayer(project, shot, dot->layer) : nullptr;
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot->piece) : nullptr;
  const bool has_rig = rig != nullptr && posed != nullptr;
  if (!has_rig) {
    return std::nullopt;
  }
  return PickedDot{posed->doll, dot->piece, dot->point, rig};
}

}  // namespace snapper
