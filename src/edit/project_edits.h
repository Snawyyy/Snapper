#ifndef SNAPPER_EDIT_PROJECT_EDITS_H_
#define SNAPPER_EDIT_PROJECT_EDITS_H_

#include <cassert>
#include <memory>
#include <utility>

#include "base/error.h"
#include "base/text.h"
#include "edit/track_ref.h"
#include "model/project.h"

namespace snapper {

// The building blocks every manager edits with. Each takes the project
// by value and hands back the next one, so an edit that fails half way
// never touches the current project.

// The project with shot id changed by change(Shot*), which may refuse.
template <typename Change>
Result<Project> WithShot(Project project, ShotId id, Change change) {
  assert(id.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const int index = ShotIndex(project, id);
  const bool is_present = index >= 0;
  if (!is_present) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  auto& shared = project.shots[static_cast<size_t>(index)];
  Shot shot = *shared;
  const Result<void> done = change(&shot);
  if (!done) {
    return std::unexpected(done.error());
  }
  shared = std::make_shared<const Shot>(std::move(shot));
  return project;
}

// The same, for one layer of a shot.
template <typename Change>
Result<Project> WithLayer(Project project, ShotId shot, LayerId id,
                          Change change) {
  assert(id.value() >= 0);
  assert(shot.value() >= 0);
  return WithShot(std::move(project), shot,
                  [&](Shot* edited) -> Result<void> {
                    Layer* layer = FindLayer(edited, id);
                    const bool is_present = layer != nullptr;
                    if (!is_present) {
                      return std::unexpected(
                          Error{Tr("That layer no longer exists.")});
                    }
                    return change(layer);
                  });
}

// The project with the channel track names changed by
// change(Channel<T>*), for any T.
template <typename Change>
Result<Project> WithTrack(Project project, const TrackRef& track,
                          Change change) {
  assert(track.shot.value() >= 0);
  assert(track.kind != TrackKind::kPiece || !track.piece.isEmpty());
  return WithShot(std::move(project), track.shot,
                  [&](Shot* shot) { return VisitTrack(shot, track, change); });
}

// The doll layer at shot and layer, or nullptr when it is gone or not
// a doll.
const DollLayer* PosedLayerOf(const Project& project, ShotId shot,
                              LayerId layer);
// The doll a doll layer shows, or nullptr when the shot, layer or doll
// is gone or the layer is not a doll.
const Doll* DollOfLayer(const Project& project, ShotId shot, LayerId layer);

// Hands out the next layer id; ids are never reused.
LayerId TakeLayerId(Project* project);
ShotId TakeShotId(Project* project);

}  // namespace snapper

#endif  // SNAPPER_EDIT_PROJECT_EDITS_H_
