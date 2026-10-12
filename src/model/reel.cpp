#include "model/reel.h"

#include <algorithm>
#include <cassert>
#include <utility>
#include <variant>

namespace snapper {

ClipSpot FindClip(const Reel& reel, ClipId id) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
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
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
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
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxClipsPerTrack > 0);
  return std::ranges::all_of(reel.tracks, [](const ReelTrack& track) {
    return track.is_sound || track.clips.empty();
  });
}

bool IsSoundClip(const Clip& clip) {
  assert(clip.length.index() >= 0);
  assert(clip.volume >= 0.0);
  return std::holds_alternative<SoundSource>(clip.source);
}

bool IsTextClip(const Clip& clip) {
  assert(clip.length.index() >= 0);
  assert(clip.in.index() >= 0);
  return std::holds_alternative<TextSource>(clip.source);
}

int SubtitleTrack(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxAllTracks > 0);
  const auto found = std::ranges::find_if(
      reel.tracks, [](const ReelTrack& track) { return track.is_subtitle; });
  const bool has_track = found != reel.tracks.end();
  return has_track ? static_cast<int>(found - reel.tracks.begin()) : -1;
}

int PictureTrackCount(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxReelTracks > 0);
  return static_cast<int>(std::ranges::count_if(
      reel.tracks, [](const ReelTrack& track) {
        return !track.is_sound && !track.is_subtitle;
      }));
}

const ReelTextLook& LookOf(const Reel& reel, const Clip& clip) {
  assert(IsTextClip(clip));
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  const auto& text = std::get<TextSource>(clip.source);
  const int subtitles = SubtitleTrack(reel);
  const bool is_cue =
      subtitles >= 0 && FindClip(reel, clip.id).track == subtitles;
  const bool is_shared = is_cue && !text.is_own_look;
  return is_shared ? reel.subtitle_look : text.look;
}

const TextStyle* FindTextStyle(const Reel& reel, const QString& name) {
  assert(reel.text_styles.size() <= static_cast<size_t>(kMaxTextStyles));
  assert(name.size() < 100000);
  const auto found = std::ranges::find_if(
      reel.text_styles,
      [&name](const TextStyle& style) { return style.name == name; });
  return found != reel.text_styles.end() ? &*found : nullptr;
}

int SoundTrackCount(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxSoundTracks > 0);
  return static_cast<int>(std::ranges::count_if(
      reel.tracks, [](const ReelTrack& track) { return track.is_sound; }));
}

bool IsTrackHeard(const Reel& reel, int track) {
  assert(track >= 0);
  assert(track < static_cast<int>(reel.tracks.size()));
  const ReelTrack& row = reel.tracks[static_cast<size_t>(track)];
  const bool has_solo = std::ranges::any_of(
      reel.tracks,
      [](const ReelTrack& other) { return other.is_sound && other.is_solo; });
  return row.is_sound && !row.is_muted && (!has_solo || row.is_solo);
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
