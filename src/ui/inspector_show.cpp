#include "ui/inspector_show.h"

#include <cassert>
#include <variant>

#include "edit/project_edits.h"

namespace snapper {
namespace {

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
  const Shot* picked_shot = FindShot(project, shot);
  const Layer* first =
      picked_shot != nullptr ? FindLayer(*picked_shot, picks.begin()->layer)
                             : nullptr;
  show.character = first != nullptr &&
                   std::holds_alternative<CharacterLayer>(first->content);
  show.layer = true;
  return show;
}

}  // namespace snapper
