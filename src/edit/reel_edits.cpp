#include "edit/reel_edits.h"

#include <algorithm>

#include "edit/project_edits.h"

namespace snapper {

bool IsTrack(const Project& project, int track) {
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(track >= -kMaxReelTracks * 2);
  return track >= 0 && track < static_cast<int>(project.reel.tracks.size());
}

ReelTrack& TrackOf(Project* project, int track) {
  assert(project != nullptr);
  assert(IsTrack(*project, track));
  return project->reel.tracks[static_cast<size_t>(track)];
}

Error InTheWay() { return Error{Tr("There's a clip in the way.")}; }

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

Result<std::vector<Lifted>> Lift(Project* project,
                                 const std::set<ClipId>& ids) {
  assert(project != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxReelTracks) *
                           kMaxClipsPerTrack);
  std::vector<Lifted> lifted;
  for (const ClipId id : ids) {
    const ClipSpot spot = FindClip(project->reel, id);
    const bool is_present = spot.IsValid();
    if (!is_present) {
      return std::unexpected(Error{Tr("That clip no longer exists.")});
    }
    auto& clips = TrackOf(project, spot.track).clips;
    lifted.push_back({spot.track, clips[static_cast<size_t>(spot.index)]});
    clips.erase(clips.begin() + spot.index);
  }
  return lifted;
}

Result<void> PlaceCopies(Project* project, const std::vector<Lifted>& copies,
                         Frame at, std::vector<ClipId>* made) {
  assert(project != nullptr);
  assert(made != nullptr);
  for (const Lifted& copy : copies) {
    const bool is_track = IsTrack(*project, copy.track);
    if (!is_track) {
      return std::unexpected(Error{Tr("That track no longer exists.")});
    }
    const int start = at.index() + copy.clip.start.index();
    const bool is_on_reel = start >= 0 && start <= kMaxFrame;
    if (!is_on_reel) {
      return std::unexpected(Error{Tr("Clips can't go past the reel.")});
    }
    ClipId id;
    auto placed = WithNewClip(*project, copy.clip.source,
                              copy.track, Frame(start), copy.clip.length,
                              &id);
    if (!placed) {
      return std::unexpected(placed.error());
    }
    *project = std::move(*placed);
    Clip& clip = TrackOf(project, copy.track)
                     .clips[static_cast<size_t>(
                         FindClip(project->reel, id).index)];
    clip.in = copy.clip.in;
    made->push_back(id);
  }
  return {};
}

QString CountLabel(size_t count, const QString& one, const QString& many) {
  assert(!one.isEmpty());
  assert(!many.isEmpty());
  return count == 1 ? one : many;
}

}  // namespace snapper
