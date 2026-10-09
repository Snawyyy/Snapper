#include "edit/change_diff.h"

#include <algorithm>
#include <cassert>
#include <iterator>

#include "edit/timeline_rows.h"

namespace snapper {
namespace {

// Channels of different kinds never hold the same key.
template <typename A, typename B>
bool SameKeyAt(const A&, Frame a_frame, const B&, Frame b_frame) {
  assert(a_frame.index() >= 0);
  assert(b_frame.index() >= 0);
  return false;
}

// True when both channels have a key (or both lack one) at the given
// frames and the keys hold the same value and ease.
template <typename T>
bool SameKeyAt(const T& a, Frame a_frame, const T& b, Frame b_frame) {
  assert(a_frame.index() >= 0);
  assert(b_frame.index() >= 0);
  const int ia = KeyIndexAt(a, a_frame);
  const int ib = KeyIndexAt(b, b_frame);
  const bool is_absent = ia < 0 || ib < 0;
  if (is_absent) {
    return ia < 0 && ib < 0;
  }
  const auto& ka = a.keys[static_cast<size_t>(ia)];
  const auto& kb = b.keys[static_cast<size_t>(ib)];
  return ka.value == kb.value && ka.ease == kb.ease;
}

// True when every channel of the row holds the same key at from before
// as at to after.
bool SamePose(const Shot& before, const Shot& after, const TimelineRow& row,
              Frame from, Frame to) {
  assert(!row.tracks.empty());
  assert(from.index() >= 0 && to.index() >= 0);
  bool is_same = true;
  for (const TrackRef& track : row.tracks) {
    ReadTrack(before, track, [&](const auto& old_channel) {
      ReadTrack(after, track, [&](const auto& new_channel) {
        is_same = is_same && SameKeyAt(old_channel, from, new_channel, to);
      });
    });
  }
  return is_same;
}

const TimelineRow* RowOf(const std::vector<TimelineRow>& rows,
                         LayerId layer) {
  assert(layer.value() >= 0);
  assert(rows.size() < 100000);
  for (const TimelineRow& row : rows) {
    const bool is_match = row.layer == layer;
    if (is_match) {
      return &row;
    }
  }
  return nullptr;
}

// Frames in a that b lacks, rising.
std::vector<Frame> Missing(const std::vector<Frame>& a,
                           const std::vector<Frame>& b) {
  assert(std::is_sorted(a.begin(), a.end()));
  assert(std::is_sorted(b.begin(), b.end()));
  std::vector<Frame> out;
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                      std::back_inserter(out));
  return out;
}

// Sorts one row's gone and new frames into moves, removals and
// additions.
void DiffRow(const Shot& before, const Shot& after,
             const TimelineRow& old_row, const TimelineRow& new_row,
             KeyChanges* out) {
  assert(out != nullptr);
  assert(old_row.layer == new_row.layer);
  std::vector<Frame> gone = Missing(old_row.keys, new_row.keys);
  std::vector<Frame> fresh = Missing(new_row.keys, old_row.keys);
  for (auto g = gone.begin(); g != gone.end();) {
    const auto twin = std::find_if(fresh.begin(), fresh.end(), [&](Frame to) {
      return SamePose(before, after, new_row, *g, to);
    });
    const bool is_move = twin != fresh.end();
    if (is_move) {
      out->moved.push_back({new_row.layer, *g, *twin});
      fresh.erase(twin);
      g = gone.erase(g);
    } else {
      ++g;
    }
  }
  for (const Frame frame : gone) {
    out->removed.push_back({new_row.layer, frame});
  }
  for (const Frame frame : fresh) {
    out->added.push_back({new_row.layer, frame});
  }
}

}  // namespace

KeyChanges DiffKeys(const Project& before, const Project& after,
                    ShotId shot) {
  assert(shot.value() >= 0);
  assert(before.shots.size() <= static_cast<size_t>(kMaxShots));
  KeyChanges changes;
  const Shot* old_shot = FindShot(before, shot);
  const Shot* new_shot = FindShot(after, shot);
  const bool has_both = old_shot != nullptr && new_shot != nullptr;
  if (!has_both) {
    return changes;
  }
  const auto old_rows = TimelineRows(before, shot);
  for (const TimelineRow& row : TimelineRows(after, shot)) {
    const TimelineRow* old_row = RowOf(old_rows, row.layer);
    const bool is_new_row = old_row == nullptr;
    if (is_new_row) {
      for (const Frame frame : row.keys) {
        changes.added.push_back({row.layer, frame});
      }
      continue;
    }
    DiffRow(*old_shot, *new_shot, *old_row, row, &changes);
  }
  return changes;
}

ClipChanges DiffClips(const Project& before, const Project& after) {
  assert(before.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(after.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  ClipChanges changes;
  for (int t = 0; t < static_cast<int>(after.reel.tracks.size()); ++t) {
    for (const Clip& clip : after.reel.tracks[static_cast<size_t>(t)].clips) {
      const ClipChange now{clip.id, t, clip.start, clip.length};
      const ClipSpot was = FindClip(before.reel, clip.id);
      const bool is_added = !was.IsValid();
      if (is_added) {
        changes.added.push_back(now);
        continue;
      }
      const Clip& old_clip = *ClipOf(before.reel, clip.id);
      const bool is_moved = was.track != t || old_clip.start != clip.start;
      if (is_moved) {
        ClipChange moved = now;
        moved.from_track = was.track;
        moved.from_start = old_clip.start;
        changes.moved.push_back(moved);
      }
    }
  }
  for (int t = 0; t < static_cast<int>(before.reel.tracks.size()); ++t) {
    for (const Clip& clip : before.reel.tracks[static_cast<size_t>(t)].clips) {
      const bool is_removed = !FindClip(after.reel, clip.id).IsValid();
      if (is_removed) {
        changes.removed.push_back({clip.id, t, clip.start, clip.length});
      }
    }
  }
  return changes;
}

}  // namespace snapper
