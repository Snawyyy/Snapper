#include "anim/reel_timeline.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
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

Frame UsedTransition(const ReelTrack& track, const Clip& clip) {
  assert(clip.length.index() >= 1);
  assert(track.clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  const Clip* next = NextTouching(track, clip);
  const bool is_joined = next != nullptr;
  return is_joined ? UsableLength(clip.out, clip.length, next->length)
                   : Frame(0);
}

Frame LongestTransition(const ReelTrack& track, const Clip& clip) {
  assert(clip.length.index() >= 1);
  assert(track.clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  const Transition widest{TransitionKind::kCrossfade, Frame(kMaxFrame)};
  const Clip* next = NextTouching(track, clip);
  const bool is_joined = next != nullptr;
  return is_joined ? UsableLength(widest, clip.length, next->length)
                   : Frame(0);
}

namespace {

// Fills in piece's mix when into (frames into its clip) falls in the
// clip's transition into the next one on track.
void MixInto(const ReelTrack& track, int into, ReelPiece* piece) {
  assert(piece != nullptr && piece->clip != nullptr);
  assert(into >= 0);
  const Clip& clip = *piece->clip;
  const int used = UsedTransition(track, clip).index();
  const int into_mix = into - (clip.length.index() - used);
  const bool is_mixing = used > 0 && into_mix >= 0;
  if (!is_mixing) {
    return;
  }
  const Clip* next = NextTouching(track, clip);
  const int before_cut = used - into_mix;
  piece->next = next;
  piece->next_source = Frame(std::max(next->in.index() - before_cut, 0));
  piece->kind = clip.out.kind;
  piece->mix = (into_mix + 1.0) / (used + 1.0);
}

}  // namespace

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
        ReelPiece piece;
        piece.track = t;
        piece.clip = &clip;
        piece.source = Frame(clip.in.index() + into);
        MixInto(project.reel.tracks[static_cast<size_t>(t)], into, &piece);
        pieces.push_back(piece);
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

std::set<ClipId> AllClips(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(kMaxClipsPerTrack > 0);
  std::set<ClipId> all;
  for (const ReelTrack& track : reel.tracks) {
    for (const Clip& clip : track.clips) {
      all.insert(clip.id);
    }
  }
  return all;
}

std::set<ClipId> ClipsIn(const Reel& reel, int low, int high, Frame first,
                         Frame last) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(first.index() >= 0 && last.index() >= 0);
  const int count = static_cast<int>(reel.tracks.size());
  const int bottom = std::clamp(std::min(low, high), 0, count);
  const int top = std::clamp(std::max(low, high), -1, count - 1);
  const Frame from = std::min(first, last);
  const Frame to = std::max(first, last);
  std::set<ClipId> caught;
  for (int t = bottom; t <= top; ++t) {
    for (const Clip& clip : reel.tracks[static_cast<size_t>(t)].clips) {
      const bool is_touched = !(to < clip.start) && from < clip.end();
      if (is_touched) {
        caught.insert(clip.id);
      }
    }
  }
  return caught;
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

Slot SlotAt(const Reel& reel, Frame at) {
  assert(at.index() >= 0);
  assert(reel.markers.size() <= static_cast<size_t>(kMaxCutMarkers));
  const auto after =
      std::upper_bound(reel.markers.begin(), reel.markers.end(), at);
  const bool has_end = after != reel.markers.end();
  if (!has_end) {
    return Slot();
  }
  const bool is_first = after == reel.markers.begin();
  return Slot{is_first ? Frame(0) : *std::prev(after), *after};
}

const Clip* SlotClip(const Reel& reel, int track, Slot slot) {
  assert(slot.IsValid());
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  const bool is_track =
      track >= 0 && track < static_cast<int>(reel.tracks.size());
  if (!is_track) {
    return nullptr;
  }
  for (const Clip& clip : reel.tracks[static_cast<size_t>(track)].clips) {
    const bool is_filling =
        clip.start == slot.start && clip.end() == slot.end;
    if (is_filling) {
      return &clip;
    }
  }
  return nullptr;
}

int LastSlotIn(const Project& project, const ClipSource& source,
               Slot slot) {
  assert(slot.IsValid());
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  Clip probe;
  probe.source = source;
  return SourceLength(project, probe).index() - slot.length().index();
}

namespace {

// How many frames at is from clip's span; 0 inside it.
int DistanceTo(const Clip& clip, Frame at) {
  assert(clip.length.index() >= 1);
  assert(at.index() >= 0);
  const int before = clip.start.index() - at.index();
  const int after = at.index() - (clip.end().index() - 1);
  return std::max({before, after, 0});
}

// The video clip on track nearest at, or nullptr when it has none.
const Clip* NearestVideo(const ReelTrack& track, Frame at) {
  assert(track.clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  assert(at.index() >= 0);
  const Clip* nearest = nullptr;
  for (const Clip& clip : track.clips) {
    const bool is_video = std::holds_alternative<VideoSource>(clip.source);
    const bool is_nearer =
        is_video && (nearest == nullptr ||
                     DistanceTo(clip, at) < DistanceTo(*nearest, at));
    if (is_nearer) {
      nearest = &clip;
    }
  }
  return nearest;
}

}  // namespace

std::optional<SlotFill> SlotFillAt(const Project& project, int track,
                                   Frame at) {
  assert(at.index() >= 0);
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  const Slot slot = SlotAt(project.reel, at);
  const bool is_track =
      track >= 0 && track < static_cast<int>(project.reel.tracks.size());
  const bool can_fill = slot.IsValid() && is_track;
  if (!can_fill) {
    return std::nullopt;
  }
  const Clip* filling = SlotClip(project.reel, track, slot);
  const bool is_filled = filling != nullptr;
  if (is_filled) {
    return SlotFill{filling->source, filling->in};
  }
  const ReelTrack& row = project.reel.tracks[static_cast<size_t>(track)];
  const Clip* video = NearestVideo(row, at);
  const bool has_video = video != nullptr;
  if (!has_video) {
    return std::nullopt;
  }
  const int played =
      video->in.index() + slot.start.index() - video->start.index();
  const int last = LastSlotIn(project, video->source, slot);
  return SlotFill{video->source, Frame(std::clamp(played, 0,
                                                  std::max(last, 0)))};
}

}  // namespace snapper
