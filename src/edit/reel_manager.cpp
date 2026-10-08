#include "edit/reel_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "media/video_reader.h"

namespace snapper {
namespace {

bool IsTrack(const Project& project, int track) {
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(track >= -kMaxReelTracks);
  return track >= 0 && track < static_cast<int>(project.reel.tracks.size());
}

ReelTrack& TrackOf(Project* project, int track) {
  assert(project != nullptr);
  assert(IsTrack(*project, track));
  return project->reel.tracks[static_cast<size_t>(track)];
}

Error InTheWay() { return Error{Tr("There's a clip in the way.")}; }

// The project with clip id lifted off its track, changed by
// change(Clip*, int* track) and put back where the change says, if it
// fits there.
template <typename Change>
Result<Project> WithClip(Project project, ClipId id, Change change) {
  assert(id.value() >= 0);
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  const ClipSpot spot = FindClip(project.reel, id);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return std::unexpected(Error{Tr("That clip no longer exists.")});
  }
  auto& clips = TrackOf(&project, spot.track).clips;
  Clip clip = clips[static_cast<size_t>(spot.index)];
  clips.erase(clips.begin() + spot.index);
  int track = spot.track;
  const Result<void> done = change(&clip, &track);
  if (!done) {
    return std::unexpected(done.error());
  }
  const bool fits = IsTrack(project, track) && clip.length.index() >= 1 &&
                    HasRoom(TrackOf(&project, track), clip);
  if (!fits) {
    return std::unexpected(InTheWay());
  }
  PlaceClip(&TrackOf(&project, track), std::move(clip));
  return project;
}

// Puts a new clip of source, length frames long, on track at start.
Result<Project> WithNewClip(Project project, ClipSource source, int track,
                            Frame start, Frame length, ClipId* id) {
  assert(id != nullptr);
  assert(length.index() >= 0);
  const bool is_track = IsTrack(project, track);
  if (!is_track) {
    return std::unexpected(Error{Tr("That track no longer exists.")});
  }
  const bool is_full = TrackOf(&project, track).clips.size() >=
                       static_cast<size_t>(kMaxClipsPerTrack);
  if (is_full) {
    return std::unexpected(
        Error{Tr("A track holds at most %1 clips.").arg(kMaxClipsPerTrack)});
  }
  Clip clip;
  clip.id = TakeClipId(&project);
  clip.source = std::move(source);
  clip.start = start;
  clip.length = Frame(std::max(length.index(), 1));
  const bool has_room = HasRoom(TrackOf(&project, track), clip);
  if (!has_room) {
    return std::unexpected(InTheWay());
  }
  *id = clip.id;
  PlaceClip(&TrackOf(&project, track), std::move(clip));
  return project;
}

// Cuts clip id in two at at, inside project; the right half gets a new
// id, which is returned.
Result<ClipId> SplitIn(Project* project, ClipId id, Frame at) {
  assert(project != nullptr);
  assert(at.index() >= 0);
  const ClipSpot spot = FindClip(project->reel, id);
  const Clip* found = spot.IsValid() ? ClipOf(project->reel, id) : nullptr;
  const bool is_inside =
      found != nullptr && found->start < at && at < found->end();
  if (!is_inside) {
    return std::unexpected(Error{Tr("Put the playhead inside the clip.")});
  }
  Clip right = *found;
  const int cut = at.index() - found->start.index();
  right.id = TakeClipId(project);
  right.start = at;
  right.in = Frame(found->in.index() + cut);
  right.length = Frame(found->length.index() - cut);
  ReelTrack& track = TrackOf(project, spot.track);
  track.clips[static_cast<size_t>(spot.index)].length = Frame(cut);
  const ClipId made = right.id;
  PlaceClip(&track, std::move(right));
  return made;
}

}  // namespace

ReelManager::ReelManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<ClipId> ReelManager::AddVideo(const QString& path, int track,
                                     Frame start) {
  assert(history_ != nullptr);
  assert(track >= -1);
  const auto info = ProbeVideo(path);
  if (!info) {
    return std::unexpected(info.error());
  }
  const Frame length(
      static_cast<int>(std::lround(info->seconds * kFramesPerSecond)));
  ClipId id;
  auto applied = history_->Apply(
      Tr("Add video"),
      WithNewClip(history_->current(), VideoSource{path, length}, track,
                  start, length, &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<ClipId> ReelManager::AddShot(ShotId shot, int track, Frame start) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool is_present = found != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  ClipId id;
  auto applied = history_->Apply(
      Tr("Add shot to video"),
      WithNewClip(history_->current(), ShotSource{shot}, track, start,
                  found->length, &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<void> ReelManager::TrimStart(ClipId clip, Frame start) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  return history_->Apply(
      Tr("Trim clip"),
      WithClip(history_->current(), clip,
               [start](Clip* edited, int*) -> Result<void> {
                 const int shift = start.index() - edited->start.index();
                 const int in = edited->in.index() + shift;
                 const int length = edited->length.index() - shift;
                 const bool is_valid = in >= 0 && length >= 1;
                 if (!is_valid) {
                   return std::unexpected(
                       Error{Tr("The clip has nothing more to show there.")});
                 }
                 edited->start = start;
                 edited->in = Frame(in);
                 edited->length = Frame(length);
                 return {};
               }));
}

Result<void> ReelManager::TrimEnd(ClipId clip, Frame end) {
  assert(history_ != nullptr);
  assert(end.index() >= 0);
  const Project& project = history_->current();
  return history_->Apply(
      Tr("Trim clip"),
      WithClip(project, clip,
               [end, &project](Clip* edited, int*) -> Result<void> {
                 const int length = end.index() - edited->start.index();
                 const int left = SourceLength(project, *edited).index() -
                                  edited->in.index();
                 // Shortening always works, even past a shot that has
                 // since got shorter; lengthening stops at the source.
                 const bool is_valid =
                     length >= 1 &&
                     (length <= edited->length.index() || length <= left);
                 if (!is_valid) {
                   return std::unexpected(
                       Error{Tr("The clip has nothing more to show there.")});
                 }
                 edited->length = Frame(length);
                 return {};
               }));
}

Result<ClipId> ReelManager::Split(ClipId clip, Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  Project next = history_->current();
  auto right = SplitIn(&next, clip, at);
  if (!right) {
    return std::unexpected(right.error());
  }
  auto applied = history_->Apply(Tr("Split clip"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return *right;
}

QString ReelManager::WhyNoSplit(const std::vector<ClipId>& clips,
                                Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool is_empty = clips.empty();
  if (is_empty) {
    return Tr("Pick a clip first.");
  }
  const Reel& reel = history_->current().reel;
  const bool is_any_inside =
      std::ranges::any_of(clips, [&reel, at](ClipId id) {
        const Clip* clip = ClipOf(reel, id);
        return clip != nullptr && clip->start < at && at < clip->end();
      });
  return is_any_inside ? QString()
                       : Tr("Put the playhead inside a picked clip.");
}

Result<std::vector<ClipId>> ReelManager::SplitAll(
    const std::vector<ClipId>& clips, Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoSplit(clips, at);
  const bool can_split = why_not.isEmpty();
  if (!can_split) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  std::vector<ClipId> halves;
  for (const ClipId clip : clips) {
    auto right = SplitIn(&next, clip, at);
    if (right) {
      halves.push_back(*right);
    }
  }
  auto applied = history_->Apply(
      halves.size() == 1 ? Tr("Split clip") : Tr("Split clips"),
      std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return halves;
}

QString ReelManager::WhyNoAddTrack() const {
  assert(history_ != nullptr);
  assert(kMaxReelTracks > 0);
  const bool is_full = history_->current().reel.tracks.size() >=
                       static_cast<size_t>(kMaxReelTracks);
  return is_full ? Tr("The video holds at most %1 tracks.").arg(kMaxReelTracks)
                 : QString();
}

QString ReelManager::WhyNoRemoveTrack(int track) const {
  assert(history_ != nullptr);
  assert(track >= -1);
  const Reel& reel = history_->current().reel;
  const int count = static_cast<int>(reel.tracks.size());
  const bool is_track = track >= 0 && track < count;
  if (!is_track) {
    return Tr("That track no longer exists.");
  }
  const bool can_go =
      count > 1 && reel.tracks[static_cast<size_t>(track)].clips.empty();
  return can_go ? QString()
                : Tr("Only an empty track can go, and one must stay.");
}

}  // namespace snapper
