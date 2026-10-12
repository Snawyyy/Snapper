#include "ui/inspector_show.h"

#include <algorithm>
#include <cassert>

#include "edit/project_edits.h"

namespace snapper {
namespace {

// Whether a picked piece (or any piece of a layer picked whole) of a doll
// has a warp grid with drag nodes or point motions.
bool HasWarpActivity(const Project& project, ShotId shot,
                     const std::set<Pick>& picks) {
  assert(picks.size() < 1000000);
  assert(shot.value() >= 0);
  for (const Pick& pick : picks) {
    const Doll* doll = DollOfLayer(project, shot, pick.layer);
    const bool is_doll = doll != nullptr;
    if (!is_doll) {
      continue;
    }
    const bool is_whole = pick.piece.isEmpty();
    for (const RigPiece& rig : doll->rig.pieces) {
      const bool is_picked = is_whole || rig.name == pick.piece;
      const bool is_active =
          rig.warp.IsOn() &&
          (!rig.drag_nodes.empty() || !rig.point_motions.empty());
      const bool is_found = is_picked && is_active;
      if (is_found) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

InspectorShow ShowFor(const Project& project, ShotId shot,
                      const std::set<Pick>& picks, bool has_dot,
                      bool is_linked, bool has_groups) {
  assert(picks.size() < 1000000);
  assert(shot.value() >= 0);
  InspectorShow show;
  const bool has_shot = FindShot(project, shot) != nullptr;
  const bool has_pick = has_shot && !picks.empty();
  show.shot = has_shot && !has_pick;
  show.camera = show.shot;
  show.pose = has_pick;
  show.link = has_pick && is_linked;
  const bool is_idle = !has_pick;
  if (is_idle) {
    return show;
  }
  const bool is_whole = std::any_of(
      picks.begin(), picks.end(),
      [](const Pick& pick) { return pick.piece.isEmpty(); });
  const bool is_doll =
      DollOfLayer(project, shot, picks.begin()->layer) != nullptr;
  show.layer = is_whole;
  show.saved_poses = is_doll && is_whole;
  show.dials = is_doll;
  show.skins = is_doll && is_whole;
  show.groups = is_doll && (has_dot || has_groups ||
                            HasWarpActivity(project, shot, picks));
  show.drag = has_dot;
  show.dot_animation = has_dot;
  return show;
}

}  // namespace snapper
