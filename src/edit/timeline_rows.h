#ifndef SNAPPER_EDIT_TIMELINE_ROWS_H_
#define SNAPPER_EDIT_TIMELINE_ROWS_H_

#include <QString>

#include <set>
#include <vector>

#include "base/frame.h"
#include "edit/track_ref.h"
#include "model/project.h"

namespace snapper {

// One row of the timeline: a whole doll (every piece and its own move),
// another layer, or the camera. Rows hold whole poses, as exposure
// columns do in traditional animation, not one channel each.
struct TimelineRow final {
  QString name;
  LayerId layer;  // Invalid for the camera row.
  std::vector<TrackRef> tracks;
  // Frames that have a key on any of the row's channels, rising.
  std::vector<Frame> keys;
  // Per key: true when the motion from it to the next key eases or
  // slides instead of holding.
  std::vector<bool> is_eased;
};

// Rows for a shot, top layer first, the camera last.
std::vector<TimelineRow> TimelineRows(const Project& project, ShotId shot);

// Every key a row has on frame, as the selection and KeyManager take
// them.
std::set<KeyRef> RowKeysAt(const Project& project, const TimelineRow& row,
                           Frame frame);

// Master frames where the picture can change: each shot's start, each
// key inside a shot, and every frame of an ease or slide. Stepping in
// animation mode jumps between these.
std::set<Frame> PoseChanges(const Project& project);

}  // namespace snapper

#endif  // SNAPPER_EDIT_TIMELINE_ROWS_H_
