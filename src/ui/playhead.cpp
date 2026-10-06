#include "ui/playhead.h"

#include <cassert>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"

namespace snapper {

std::optional<PlayheadSpot> SpotOf(const Managers& managers) {
  assert(managers.history != nullptr);
  assert(managers.playback != nullptr);
  const Project& project = managers.history->current();
  const ShotMoment moment = Locate(project, managers.playback->frame());
  const bool is_on_shot = moment.shot >= 0;
  if (!is_on_shot) {
    return std::nullopt;
  }
  // In a hand-over, edits go to whichever side is picked.
  const bool is_next_picked =
      moment.next_shot >= 0 &&
      project.shots[static_cast<size_t>(moment.next_shot)]->id ==
          managers.selection->shot();
  return is_next_picked
             ? PlayheadSpot{project.shots[static_cast<size_t>(
                                moment.next_shot)]->id,
                            moment.next_local}
             : PlayheadSpot{
                   project.shots[static_cast<size_t>(moment.shot)]->id,
                   moment.local};
}

std::optional<TrackRef> PickedTrack(const Managers& managers, ShotId shot) {
  const SelectionManager* selection = managers.selection;
  assert(selection != nullptr);
  assert(shot.value() >= 0);
  const bool is_picked =
      selection->shot() == shot && selection->layer().IsValid();
  if (!is_picked) {
    return std::nullopt;
  }
  const bool has_piece = !selection->pieces().empty();
  return has_piece ? TrackRef{shot, TrackKind::kPiece, selection->layer(),
                              *selection->pieces().begin()}
                   : TrackRef{shot, TrackKind::kLayer, selection->layer(),
                              {}};
}

}  // namespace snapper
