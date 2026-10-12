#ifndef SNAPPER_EDIT_CHANGE_DIFF_H_
#define SNAPPER_EDIT_CHANGE_DIFF_H_

#include <vector>

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// One key on a timeline row: the layer whose row it sits on (invalid for
// the camera row) and its frame.
struct KeyChange final {
  LayerId layer;
  Frame frame;

  bool operator==(const KeyChange&) const = default;
};

// A key that kept its pose and moved to another frame of the same row.
struct KeyMove final {
  LayerId layer;
  Frame from;
  Frame to;

  bool operator==(const KeyMove&) const = default;
};

// What an edit did to the keys of one shot's timeline rows.
struct KeyChanges final {
  // Keys the edit made; none of them is the end of a move.
  std::vector<KeyChange> added;
  // Keys the edit took away; none of them is the start of a move.
  std::vector<KeyChange> removed;
  std::vector<KeyMove> moved;

  bool IsEmpty() const {
    return added.empty() && removed.empty() && moved.empty();
  }
};

// Compares a shot before and after an edit, row by row. A key gone from
// one frame and a key of the same pose and ease new on another frame of
// the same row count as one move. A layer that only the later side has
// counts its keys as added (it is new); one that only the earlier side
// has says nothing, as its row is no longer there to show it. Pure: it
// only reads.
KeyChanges DiffKeys(const Project& before, const Project& after, ShotId shot);

// A clip on the reel with the place it holds: after the edit for an
// added or moved clip, before it for a removed one.
struct ClipChange final {
  ClipId clip;
  int track = 0;
  Frame start;
  Frame length;
  // For a moved clip: the track and start it came from; -1 otherwise.
  int from_track = -1;
  Frame from_start = Frame(0);

  bool operator==(const ClipChange&) const = default;
};

// What an edit did to the reel's clips, found by clip id.
struct ClipChanges final {
  std::vector<ClipChange> added;
  std::vector<ClipChange> removed;
  // Clips that changed track or start frame.
  std::vector<ClipChange> moved;

  bool IsEmpty() const {
    return added.empty() && removed.empty() && moved.empty();
  }
};

ClipChanges DiffClips(const Project& before, const Project& after);

// The layers of one shot that both projects have and that differ
// between them, in the later project's order. A layer only one side has
// is left out: the stage has nothing to flash for it.
std::vector<LayerId> ChangedLayers(const Project& before,
                                   const Project& after, ShotId shot);

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHANGE_DIFF_H_
