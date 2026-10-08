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
  // Posing always works at the shots' own playhead, even while the
  // reel's is the one moving.
  const ShotMoment moment =
      Locate(project, managers.playback->FrameOn(Timeline::kShots));
  const bool is_on_shot = moment.shot >= 0;
  if (!is_on_shot) {
    return std::nullopt;
  }
  return PlayheadSpot{project.shots[static_cast<size_t>(moment.shot)]->id,
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
