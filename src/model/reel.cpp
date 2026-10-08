#include "model/reel.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace snapper {

ClipSpot FindClip(const Reel& reel, ClipId id) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(id.value() >= 0);
  const int track_count = static_cast<int>(reel.tracks.size());
  for (int t = 0; t < track_count; ++t) {
    const auto& clips = reel.tracks[static_cast<size_t>(t)].clips;
    const int clip_count = static_cast<int>(clips.size());
    for (int i = 0; i < clip_count; ++i) {
      const bool is_match = clips[static_cast<size_t>(i)].id == id;
      if (is_match) {
        return ClipSpot{t, i};
      }
    }
  }
  return ClipSpot();
}

const Clip* ClipOf(const Reel& reel, ClipId id) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(id.value() >= 0);
  const ClipSpot spot = FindClip(reel, id);
  const bool is_found = spot.IsValid();
  return is_found ? &reel.tracks[static_cast<size_t>(spot.track)]
                         .clips[static_cast<size_t>(spot.index)]
                  : nullptr;
}

bool HasRoom(const ReelTrack& track, const Clip& clip) {
  assert(track.clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  assert(clip.length.index() >= 1);
  for (const Clip& other : track.clips) {
    const bool is_self = other.id == clip.id;
    const bool is_overlap =
        other.start < clip.end() && clip.start < other.end();
    const bool is_blocked = !is_self && is_overlap;
    if (is_blocked) {
      return false;
    }
  }
  return true;
}

void PlaceClip(ReelTrack* track, Clip clip) {
  assert(track != nullptr);
  assert(HasRoom(*track, clip));
  const auto is_later = [&clip](const Clip& other) {
    return clip.start < other.start;
  };
  const auto at = std::ranges::find_if(track->clips, is_later);
  track->clips.insert(at, std::move(clip));
}

bool IsReelEmpty(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(kMaxClipsPerTrack > 0);
  return std::ranges::all_of(
      reel.tracks, [](const ReelTrack& track) { return track.clips.empty(); });
}

const Clip* NextTouching(const ReelTrack& track, const Clip& clip) {
  assert(track.clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  assert(clip.length.index() >= 1);
  const auto touching = std::ranges::find_if(
      track.clips, [&clip](const Clip& other) {
        return other.start == clip.end() && other.id != clip.id;
      });
  const bool is_found = touching != track.clips.end();
  return is_found ? &*touching : nullptr;
}

}  // namespace snapper
