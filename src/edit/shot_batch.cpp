// ShotManager's changes to many shots at once.

#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/shot_manager.h"

namespace snapper {
namespace {

// The project with change(Shot*) applied to each of shots; none of them
// left is refused.
template <typename Change>
Result<Project> WithShots(Project project, const std::vector<ShotId>& shots,
                          Change change) {
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  int changed = 0;
  for (const ShotId id : shots) {
    auto next = WithShot(project, id, [&](Shot* shot) {
      change(shot);
      return Result<void>();
    });
    const bool is_found = next.has_value();
    if (is_found) {
      project = std::move(*next);
      ++changed;
    }
  }
  const bool is_none = changed == 0;
  if (is_none) {
    return std::unexpected(Error{Tr("Pick one or more shots first.")});
  }
  return project;
}

}  // namespace

Result<void> ShotManager::RemoveAll(const std::vector<ShotId>& shots) {
  assert(history_ != nullptr);
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  Project next = history_->current();
  const auto removed = std::erase_if(next.shots, [&shots](const auto& shot) {
    return std::find(shots.begin(), shots.end(), shot->id) != shots.end();
  });
  const bool is_removed = removed > 0;
  if (!is_removed) {
    return std::unexpected(Error{Tr("Pick one or more shots first.")});
  }
  return history_->Apply(Tr("Remove shots"), std::move(next));
}

Result<void> ShotManager::DuplicateAll(const std::vector<ShotId>& shots) {
  assert(history_ != nullptr);
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  EditScope batch(history_, Tr("Duplicate shots"));
  for (const ShotId id : shots) {
    const auto copy = Duplicate(id);
    if (!copy) {
      batch.Cancel();
      return std::unexpected(copy.error());
    }
  }
  return {};
}

Result<void> ShotManager::ShiftLength(const std::vector<ShotId>& shots,
                                      int delta) {
  assert(history_ != nullptr);
  assert(std::abs(delta) <= kMaxFrame);
  return history_->Apply(
      Tr("Change shot length"),
      WithShots(history_->current(), shots, [delta](Shot* shot) {
        shot->length = Frame(std::max(1, shot->length.index() + delta));
      }));
}

Result<void> ShotManager::SetBackgroundAll(const std::vector<ShotId>& shots,
                                           QColor color) {
  assert(history_ != nullptr);
  const bool is_valid = color.isValid();
  if (!is_valid) {
    return std::unexpected(Error{Tr("That isn't a colour.")});
  }
  return history_->Apply(
      Tr("Change background"),
      WithShots(history_->current(), shots,
                [color](Shot* shot) { shot->background = color; }));
}

Result<void> ShotManager::SetTransitionAll(const std::vector<ShotId>& shots,
                                           Transition transition) {
  assert(history_ != nullptr);
  assert(static_cast<int>(transition.kind) < kTransitionKindCount);
  return history_->Apply(
      Tr("Change transition"),
      WithShots(history_->current(), shots,
                [transition](Shot* shot) { shot->transition = transition; }));
}

}  // namespace snapper
