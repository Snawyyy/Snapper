#include "anim/reel_timeline.h"

#include <algorithm>
#include <cassert>
#include <variant>

namespace snapper {

Frame SourceLength(const Project& project, const Clip& clip) {
  assert(clip.length.index() >= 1);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const auto* video = std::get_if<VideoSource>(&clip.source);
  const bool is_video = video != nullptr;
  if (is_video) {
    return video->length;
  }
  const Shot* shot = FindShot(project, std::get<ShotSource>(clip.source).shot);
  const bool is_gone = shot == nullptr;
  return is_gone ? Frame(0) : shot->length;
}

Frame ShownLength(const Project& project, const Clip& clip) {
  assert(clip.length.index() >= 1);
  assert(clip.in.index() >= 0);
  const int left = SourceLength(project, clip).index() - clip.in.index();
  return Frame(std::min(left, clip.length.index()));
}

std::vector<ReelPiece> ReelAt(const Project& project, Frame frame) {
  assert(frame.index() >= 0);
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  std::vector<ReelPiece> pieces;
  const int track_count = static_cast<int>(project.reel.tracks.size());
  for (int t = 0; t < track_count; ++t) {
    for (const Clip& clip : project.reel.tracks[static_cast<size_t>(t)].clips) {
      const int into = frame.index() - clip.start.index();
      const bool is_shown =
          into >= 0 && into < ShownLength(project, clip).index();
      if (is_shown) {
        pieces.push_back({t, &clip, Frame(clip.in.index() + into)});
        break;
      }
    }
  }
  assert(pieces.size() <= project.reel.tracks.size());
  return pieces;
}

Frame ReelLength(const Project& project) {
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(kMaxClipsPerTrack > 0);
  Frame end;
  for (const ReelTrack& track : project.reel.tracks) {
    const bool has_clips = !track.clips.empty();
    if (has_clips) {
      // Sorted and never overlapping: the last clip ends last.
      end = std::max(end, track.clips.back().end());
    }
  }
  return end;
}

Frame TrackEnd(const Project& project, int track) {
  assert(track >= 0);
  assert(track < static_cast<int>(project.reel.tracks.size()));
  const auto& clips = project.reel.tracks[static_cast<size_t>(track)].clips;
  return clips.empty() ? Frame(0) : clips.back().end();
}

std::set<Frame> ReelCuts(const Project& project) {
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(kMaxClipsPerTrack > 0);
  std::set<Frame> cuts;
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      cuts.insert(clip.start);
      cuts.insert(clip.end());
    }
  }
  return cuts;
}

}  // namespace snapper
