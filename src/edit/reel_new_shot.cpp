// ReelManager's New shot here: a blank shot made to the size of a gap or
// a marked range and laid on the reel over it, as one step.

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"

namespace snapper {
namespace {

bool IsPictureTrack(const Reel& reel, int track) {
  assert(track >= -1);
  assert(reel.tracks.size() < 100000);
  const bool is_track =
      track >= 0 && track < static_cast<int>(reel.tracks.size());
  if (!is_track) {
    return false;
  }
  const ReelTrack& row = reel.tracks[static_cast<size_t>(track)];
  return !row.is_sound && !row.is_subtitle;
}

bool HasRoomFor(const Reel& reel, int track, Frame from, Frame to) {
  assert(to > from);
  assert(IsPictureTrack(reel, track));
  Clip probe;
  probe.start = from;
  probe.length = Frame(to.index() - from.index());
  return HasRoom(reel.tracks[static_cast<size_t>(track)], probe);
}

// track itself when it is a picture track with room from to to; for -1
// the lowest such track; else -1.
int TrackWithRoom(const Reel& reel, int track, Frame from, Frame to) {
  assert(track >= -1);
  assert(to > from);
  const bool is_given = track >= 0;
  if (is_given) {
    const bool is_usable = IsPictureTrack(reel, track) &&
                           HasRoomFor(reel, track, from, to);
    return is_usable ? track : -1;
  }
  const int count = static_cast<int>(reel.tracks.size());
  for (int row = 0; row < count; ++row) {
    const bool is_usable =
        IsPictureTrack(reel, row) && HasRoomFor(reel, row, from, to);
    if (is_usable) {
      return row;
    }
  }
  return -1;
}

}  // namespace

QString ReelManager::WhyNoNewShot(int track, Frame from, Frame to) const {
  assert(history_ != nullptr);
  assert(track >= -1);
  const Project& now = history_->current();
  const bool is_span = from.index() >= 0 && to > from;
  if (!is_span) {
    return Tr("Mark a stretch of the video or a gap between cuts first.");
  }
  const bool is_full = now.shots.size() >= static_cast<size_t>(kMaxShots);
  if (is_full) {
    return Tr("A project holds at most %1 shots.").arg(kMaxShots);
  }
  const bool is_wrong_track = track >= 0 && !IsPictureTrack(now.reel, track);
  if (is_wrong_track) {
    return Tr("A shot goes on a picture track.");
  }
  const bool has_room = TrackWithRoom(now.reel, track, from, to) >= 0;
  return has_room ? QString() : Tr("Another clip is in the way there.");
}

Result<ClipId> ReelManager::AddNewShot(int track, Frame from, Frame to) {
  assert(history_ != nullptr);
  assert(track >= -1);
  const QString why_not = WhyNoNewShot(track, from, to);
  const bool can_add = why_not.isEmpty();
  if (!can_add) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  const int row = TrackWithRoom(next.reel, track, from, to);
  assert(row >= 0);
  Shot shot;
  shot.id = TakeShotId(&next);
  shot.name = Tr("Shot %1").arg(shot.id.value());
  shot.is_transparent = next.new_shots_transparent;
  shot.length = Frame(to.index() - from.index());
  next.shots.push_back(std::make_shared<const Shot>(shot));
  ClipId id;
  auto applied = history_->Apply(
      Tr("New shot here"), WithNewClip(std::move(next), ShotSource{shot.id},
                                       row, from, shot.length, &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Slot ReelManager::NewShotSpan(int track, Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Project& now = history_->current();
  const Slot marked = SlotAt(now.reel, at);
  const bool is_usable_track = track >= 0 && IsPictureTrack(now.reel, track);
  if (!is_usable_track) {
    return marked.IsValid() ? marked
                            : Slot{at, Frame(at.index() + kNewShotFrames)};
  }
  const ReelTrack& row = now.reel.tracks[static_cast<size_t>(track)];
  Frame before(0);
  Frame after(at.index() + kNewShotFrames);
  bool is_bounded = false;
  for (const Clip& clip : row.clips) {
    const bool is_on = clip.start <= at && at < clip.end();
    if (is_on) {
      return Slot{};
    }
    if (clip.end() <= at) {
      before = std::max(before, clip.end());
    } else if (!is_bounded || clip.start < after) {
      after = clip.start;
      is_bounded = true;
    }
  }
  if (marked.IsValid()) {
    // The gap between markers, cut back to the free space it shares.
    return Slot{std::max(marked.start, before), std::min(marked.end, after)};
  }
  // Free space with no clip after it: a new shot of the usual length
  // from at.
  return is_bounded ? Slot{before, after}
                    : Slot{at, Frame(at.index() + kNewShotFrames)};
}

}  // namespace snapper
