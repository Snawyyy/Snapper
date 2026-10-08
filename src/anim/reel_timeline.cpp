#include "anim/reel_timeline.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
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

std::set<Frame> ReelCuts(const Project& project,
                         const std::set<ClipId>& ignore) {
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(ignore.size() < 1000000);
  std::set<Frame> cuts;
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      const bool is_ignored = ignore.contains(clip.id);
      if (!is_ignored) {
        cuts.insert(clip.start);
        cuts.insert(clip.end());
      }
    }
  }
  return cuts;
}

namespace {

// How far edge is from the nearest of points within reach, or 0 with
// *found false when none is.
int Pull(const std::set<Frame>& points, int edge, int reach, bool* found) {
  assert(found != nullptr);
  assert(reach >= 0);
  int best = 0;
  *found = false;
  for (const Frame point : points) {
    const int gap = point.index() - edge;
    const bool is_nearer = std::abs(gap) <= reach &&
                           (!*found || std::abs(gap) < std::abs(best));
    if (is_nearer) {
      best = gap;
      *found = true;
    }
  }
  return best;
}

std::set<Frame> SnapPoints(const Project& project,
                           const std::set<ClipId>& ignore, Frame playhead) {
  assert(playhead.index() >= 0);
  assert(ignore.size() < 1000000);
  std::set<Frame> points = ReelCuts(project, ignore);
  points.insert(project.reel.markers.begin(), project.reel.markers.end());
  points.insert(playhead);
  points.insert(Frame(0));
  return points;
}

}  // namespace

Frame SnapFrame(const Project& project, Frame at, int reach,
                const std::set<ClipId>& ignore, Frame playhead) {
  assert(reach >= 0);
  assert(at.index() >= 0);
  bool found = false;
  const int pull = Pull(SnapPoints(project, ignore, playhead), at.index(),
                        reach, &found);
  return Frame(at.index() + pull);
}

int SnapDelta(const Project& project, const std::set<ClipId>& moving,
              int delta, int reach, Frame playhead) {
  assert(reach >= 0);
  assert(moving.size() < 1000000);
  const std::set<Frame> points = SnapPoints(project, moving, playhead);
  int best = 0;
  bool has_best = false;
  for (const ClipId id : moving) {
    const Clip* clip = ClipOf(project.reel, id);
    const bool is_present = clip != nullptr;
    if (!is_present) {
      continue;
    }
    for (const int edge : {clip->start.index(), clip->end().index()}) {
      bool found = false;
      const int pull = Pull(points, edge + delta, reach, &found);
      const bool is_better =
          found && (!has_best || std::abs(pull) < std::abs(best));
      if (is_better) {
        best = pull;
        has_best = true;
      }
    }
  }
  return delta + best;
}

}  // namespace snapper
