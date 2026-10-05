#ifndef SNAPPER_UI_PLAYHEAD_H_
#define SNAPPER_UI_PLAYHEAD_H_

#include <optional>

#include "base/frame.h"
#include "edit/track_ref.h"
#include "ui/managers.h"

namespace snapper {

// The shot under the playhead and the frame inside it, where every
// posing panel keys; nothing past the last shot.
struct PlayheadSpot final {
  ShotId shot;
  Frame local;
};
std::optional<PlayheadSpot> SpotOf(const Managers& managers);

// The channel the user is working on: the picked piece, or the picked
// layer's own move; nothing when no layer of shot is picked.
std::optional<TrackRef> PickedTrack(const Managers& managers, ShotId shot);

}  // namespace snapper

#endif  // SNAPPER_UI_PLAYHEAD_H_
