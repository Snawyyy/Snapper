#ifndef SNAPPER_EDIT_REEL_EDITS_H_
#define SNAPPER_EDIT_REEL_EDITS_H_

#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "base/error.h"
#include "base/text.h"
#include "model/project.h"

namespace snapper {

// The building blocks ReelManager's edits share. Like project_edits.h,
// each works on a project value, so an edit that fails half way never
// touches the current project.

bool IsTrack(const Project& project, int track);
ReelTrack& TrackOf(Project* project, int track);
Error InTheWay();

// A clip taken off its track, and the track it came from.
struct Lifted final {
  int track = -1;
  Clip clip;
};

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
                            Frame start, Frame length, ClipId* id);

// Cuts clip id in two at at, inside project; the right half gets a new
// id, which is returned.
Result<ClipId> SplitIn(Project* project, ClipId id, Frame at);

// Takes every clip in ids off its track, or says which is gone.
Result<std::vector<Lifted>> Lift(Project* project,
                                 const std::set<ClipId>& ids);

// Lays copies (starts counted from 0) onto their tracks from at, each
// with a new id; the ids go to made, in order.
Result<void> PlaceCopies(Project* project, const std::vector<Lifted>& copies,
                         Frame at, std::vector<ClipId>* made);

// "one" for a single clip, else "many": undo names say how many.
QString CountLabel(size_t count, const QString& one, const QString& many);

}  // namespace snapper

#endif  // SNAPPER_EDIT_REEL_EDITS_H_
