#include "ui/picked_dot.h"

#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"

namespace snapper {

namespace {

std::optional<PickedDot> Resolve(const Managers& managers,
                                 const WarpDot& dot) {
  assert(managers.selection != nullptr);
  assert(managers.history != nullptr);
  const Project& project = managers.history->current();
  const ShotId shot = managers.selection->shot();
  const DollLayer* posed = PosedLayerOf(project, shot, dot.layer);
  const Doll* doll = DollOfLayer(project, shot, dot.layer);
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot.piece) : nullptr;
  const bool has_rig = rig != nullptr && posed != nullptr;
  if (!has_rig) {
    return std::nullopt;
  }
  return PickedDot{posed->doll, dot.layer, dot.piece, dot.point,
                   std::make_shared<const RigPiece>(PieceFor(*rig, dot.layer))};
}

}  // namespace

std::optional<PickedDot> PickedDotOf(const Managers& managers) {
  assert(managers.selection != nullptr);
  assert(managers.history != nullptr);
  const auto dot = managers.selection->dot();
  return dot ? Resolve(managers, *dot) : std::nullopt;
}

std::vector<PickedDot> PickedDotsOf(const Managers& managers) {
  assert(managers.selection != nullptr);
  assert(managers.history != nullptr);
  std::vector<PickedDot> picked;
  const auto focus = PickedDotOf(managers);
  if (focus) {
    picked.push_back(*focus);
  }
  for (const WarpDot& dot : managers.selection->dots()) {
    const auto found = Resolve(managers, dot);
    const bool is_other =
        found && !(focus && focus->piece == found->piece &&
                   focus->point == found->point && focus->doll == found->doll);
    if (is_other) {
      picked.push_back(*found);
    }
  }
  return picked;
}

}  // namespace snapper
