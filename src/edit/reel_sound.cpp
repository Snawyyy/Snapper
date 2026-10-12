// ReelManager's sound: sound clips, their volume and fades, and the
// sound tracks' headers.

#include <QFileInfo>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <variant>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"
#include "media/audio_clip.h"

namespace snapper {
namespace {

// Room on track for length frames from start.
bool HasRoomAt(const ReelTrack& track, Frame start, Frame length) {
  assert(start.index() >= 0);
  assert(length.index() >= 1);
  Clip probe;
  probe.start = start;
  probe.length = length;
  return HasRoom(track, probe);
}

// The first sound track with room for length frames from start, or -1.
int FreeSoundTrack(const Reel& reel, Frame start, Frame length) {
  assert(start.index() >= 0);
  assert(length.index() >= 1);
  const int count = static_cast<int>(reel.tracks.size());
  for (int t = 0; t < count; ++t) {
    const ReelTrack& track = reel.tracks[static_cast<size_t>(t)];
    const bool is_free =
        track.is_sound && HasRoomAt(track, start, length);
    if (is_free) {
      return t;
    }
  }
  return -1;
}

// A new empty sound track on top of the sound tracks; its index.
int InsertSoundTrack(Reel* reel) {
  assert(reel != nullptr);
  assert(SoundTrackCount(*reel) < kMaxSoundTracks);
  const int at = SoundTrackCount(*reel);
  ReelTrack track;
  track.is_sound = true;
  reel->tracks.insert(reel->tracks.begin() + at, std::move(track));
  return at;
}

bool IsSoundTrack(const Project& project, int track) {
  assert(track >= -kMaxAllTracks * 2);
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  return IsTrack(project, track) &&
         project.reel.tracks[static_cast<size_t>(track)].is_sound;
}

Error NoSoundTrack() { return Error{Tr("That is not a sound track.")}; }

Frame FramesOfSeconds(double seconds) {
  assert(std::isfinite(seconds));
  assert(kFramesPerSecond > 0);
  return Frame(std::max(
      static_cast<int>(std::lround(seconds * kFramesPerSecond)), 1));
}

// Lays the project's song from frame 0 on the first sound track with
// room for it (a new one when none has); nothing without a readable
// song.
void LaySong(Project* project) {
  assert(project != nullptr);
  assert(project->reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  const bool has_song = !project->song.isEmpty();
  if (!has_song) {
    return;
  }
  const QString song = QFileInfo(project->song).absoluteFilePath();
  const auto seconds = ProbeSoundSeconds(song);
  if (!seconds) {
    return;
  }
  const Frame length = FramesOfSeconds(*seconds);
  int track = FreeSoundTrack(project->reel, Frame(0), length);
  const bool needs_track =
      track < 0 && SoundTrackCount(project->reel) < kMaxSoundTracks;
  if (needs_track) {
    track = InsertSoundTrack(&project->reel);
  }
  if (track < 0) {
    return;
  }
  ClipId id;
  auto placed = WithNewClip(*project, SoundSource{song, length}, track,
                            Frame(0), length, &id);
  if (placed) {
    *project = std::move(*placed);
  }
}

}  // namespace

Result<SoundSource> ReelManager::ReadSound(const QString& path) const {
  assert(history_ != nullptr);
  assert(kFramesPerSecond > 0);
  const auto seconds = ProbeSoundSeconds(path);
  if (!seconds) {
    return std::unexpected(seconds.error());
  }
  return SoundSource{QFileInfo(path).absoluteFilePath(),
                     FramesOfSeconds(*seconds)};
}

Result<std::vector<ClipId>> ReelManager::AddSounds(const QStringList& paths,
                                                   int track, Frame start) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  const bool is_empty = paths.isEmpty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick a sound file first.")});
  }
  std::vector<SoundSource> sounds;
  int total = 0;
  for (const QString& path : paths) {
    auto sound = ReadSound(path);
    if (!sound) {
      return std::unexpected(sound.error());
    }
    total += sound->length.index();
    sounds.push_back(std::move(*sound));
  }
  Project next = history_->current();
  const Frame row(std::max(total, 1));
  const bool is_named_track = IsSoundTrack(next, track);
  int target = is_named_track ? track : FreeSoundTrack(next.reel, start, row);
  const bool needs_track =
      target < 0 && SoundTrackCount(next.reel) < kMaxSoundTracks;
  if (needs_track) {
    target = InsertSoundTrack(&next.reel);
  }
  if (target < 0) {
    return std::unexpected(InTheWay());
  }
  std::vector<ClipId> made;
  Frame at = start;
  for (const SoundSource& sound : sounds) {
    ClipId id;
    auto placed =
        WithNewClip(std::move(next), sound, target, at, sound.length, &id);
    if (!placed) {
      return std::unexpected(placed.error());
    }
    next = std::move(*placed);
    made.push_back(id);
    at = Frame(at.index() + sound.length.index());
  }
  auto applied = history_->Apply(
      CountLabel(made.size(), Tr("Add sound"), Tr("Add sounds")),
      std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return made;
}

Result<void> ReelManager::AddSoundTrack() {
  assert(history_ != nullptr);
  const QString why_not = WhyNoAddSoundTrack();
  const bool can_add = why_not.isEmpty();
  if (!can_add) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  // On top of the sound tracks, under the picture ones.
  InsertSoundTrack(&next.reel);
  return history_->Apply(Tr("Add sound track"), std::move(next));
}

Result<void> ReelManager::SetClipVolume(ClipId clip, double gain) {
  assert(history_ != nullptr);
  assert(std::isfinite(gain));
  const double kept = std::clamp(gain, 0.0, kMaxGain);
  return history_->Apply(
      Tr("Change volume"),
      WithClip(history_->current(), clip,
               [kept](Clip* edited, int*) -> Result<void> {
                 if (!IsSoundClip(*edited)) {
                   return std::unexpected(
                       Error{Tr("Only sound clips have a volume.")});
                 }
                 edited->volume = kept;
                 return {};
               }));
}

Result<void> ReelManager::SetFadeIn(ClipId clip, Frame frames) {
  assert(history_ != nullptr);
  assert(frames.index() >= 0);
  return history_->Apply(
      Tr("Fade in"),
      WithClip(history_->current(), clip,
               [frames](Clip* edited, int*) -> Result<void> {
                 if (!IsSoundClip(*edited)) {
                   return std::unexpected(
                       Error{Tr("Only sound clips fade.")});
                 }
                 const int room =
                     edited->length.index() - edited->fade_out.index();
                 edited->fade_in = Frame(std::clamp(frames.index(), 0, room));
                 return {};
               }));
}

Result<void> ReelManager::SetFadeOut(ClipId clip, Frame frames) {
  assert(history_ != nullptr);
  assert(frames.index() >= 0);
  return history_->Apply(
      Tr("Fade out"),
      WithClip(history_->current(), clip,
               [frames](Clip* edited, int*) -> Result<void> {
                 if (!IsSoundClip(*edited)) {
                   return std::unexpected(
                       Error{Tr("Only sound clips fade.")});
                 }
                 const int room =
                     edited->length.index() - edited->fade_in.index();
                 edited->fade_out =
                     Frame(std::clamp(frames.index(), 0, room));
                 return {};
               }));
}

Result<void> ReelManager::SetTrackVolume(int track, double gain) {
  assert(history_ != nullptr);
  assert(std::isfinite(gain));
  Project next = history_->current();
  if (!IsSoundTrack(next, track)) {
    return std::unexpected(NoSoundTrack());
  }
  TrackOf(&next, track).volume = std::clamp(gain, 0.0, kMaxGain);
  return history_->Apply(Tr("Change track volume"), std::move(next));
}

Result<void> ReelManager::SetTrackMuted(int track, bool is_muted) {
  assert(history_ != nullptr);
  assert(track >= -1);
  Project next = history_->current();
  if (!IsSoundTrack(next, track)) {
    return std::unexpected(NoSoundTrack());
  }
  TrackOf(&next, track).is_muted = is_muted;
  return history_->Apply(is_muted ? Tr("Mute track") : Tr("Unmute track"),
                         std::move(next));
}

Result<void> ReelManager::SetTrackSolo(int track, bool is_solo) {
  assert(history_ != nullptr);
  assert(track >= -1);
  Project next = history_->current();
  if (!IsSoundTrack(next, track)) {
    return std::unexpected(NoSoundTrack());
  }
  TrackOf(&next, track).is_solo = is_solo;
  return history_->Apply(is_solo ? Tr("Solo track") : Tr("Unsolo track"),
                         std::move(next));
}

void ReelManager::AddSoundTracks(Project* project) {
  assert(project != nullptr);
  assert(project->reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  const bool has_tracks = SoundTrackCount(project->reel) > 0;
  if (has_tracks) {
    return;
  }
  for (int i = 0; i < kDefaultSoundTracks; ++i) {
    InsertSoundTrack(&project->reel);
  }
  LaySong(project);
}

void ReelManager::ReplaceSong(Project* project, const QString& old_song) {
  assert(project != nullptr);
  assert(old_song.size() < 100000);
  const bool has_old = !old_song.isEmpty();
  for (ReelTrack& track : project->reel.tracks) {
    std::erase_if(track.clips, [&](const Clip& clip) {
      const auto* sound = std::get_if<SoundSource>(&clip.source);
      return has_old && sound != nullptr && sound->path == old_song;
    });
  }
  LaySong(project);
}

}  // namespace snapper
