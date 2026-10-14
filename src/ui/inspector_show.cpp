#include "ui/inspector_show.h"

#include <algorithm>
#include <cassert>
#include <variant>

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

// Whether the first pick is a storyboard layer: it has no pose, links or
// poses to show, only its own box and the layer's name and frames.
bool IsStoryboardPick(const Project& project, ShotId shot,
                      const std::set<Pick>& picks) {
  assert(picks.size() < 1000000);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const bool can_look = found != nullptr && !picks.empty();
  const Layer* layer =
      can_look ? FindLayer(*found, picks.begin()->layer) : nullptr;
  const bool has_layer = layer != nullptr;
  return has_layer && std::holds_alternative<StoryboardLayer>(layer->content);
}

}  // namespace

InspectorShow ShowFor(const Project& project, ShotId shot,
                      const std::set<Pick>& picks, const PickExtras& extras) {
  assert(picks.size() < 1000000);
  assert(shot.value() >= 0);
  InspectorShow show;
  const bool has_shot = FindShot(project, shot) != nullptr;
  const bool has_pick = has_shot && !picks.empty();
  show.shot = has_shot && !has_pick;
  show.camera = show.shot;
  const bool is_board = IsStoryboardPick(project, shot, picks);
  show.pose = has_pick && !is_board;
  show.link = has_pick && !is_board && extras.is_linked;
  show.references = has_shot && extras.has_reference;
  show.storyboard = has_shot && extras.has_storyboard;
  const bool is_idle = !has_pick;
  if (is_idle) {
    return show;
  }
  const bool is_whole = std::any_of(
      picks.begin(), picks.end(),
      [](const Pick& pick) { return pick.piece.isEmpty(); });
  const Doll* doll = DollOfLayer(project, shot, picks.begin()->layer);
  const bool is_doll = doll != nullptr;
  show.object = is_doll && is_whole && doll->rig.object.is_object;
  show.layer = is_whole;
  show.saved_poses = is_doll && is_whole;
  const bool has_dials = is_doll && !doll->rig.dials.empty();
  show.dials = has_dials;
  show.add_dial = is_doll && !has_dials;
  show.skins = is_doll && is_whole && !doll->rig.skins.empty();
  show.groups = is_doll && (extras.has_dot || extras.has_groups ||
                            HasWarpActivity(project, shot, picks));
  show.drag = extras.has_dot;
  show.dot_animation = extras.has_dot;
  return show;
}

}  // namespace snapper
