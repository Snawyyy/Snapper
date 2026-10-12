// HistoryManager: undo and redo for one shot.

#include <cassert>
#include <utility>

#include "edit/history_manager.h"

namespace snapper {

std::optional<size_t> HistoryManager::StepFor(const std::deque<Step>& steps,
                                              ShotId shot) const {
  assert(steps.size() <= static_cast<size_t>(kMaxUndoSteps) + 1);
  assert(shot.value() >= 0);
  const bool is_empty = steps.empty();
  if (is_empty) {
    return std::nullopt;
  }
  // The newest change, when it is not inside one shot, is for any shot.
  const bool is_shared_on_top = steps.back().scope.is_shared;
  if (is_shared_on_top) {
    return steps.size() - 1;
  }
  for (size_t i = steps.size(); i-- > 0;) {
    const StepScope& scope = steps[i].scope;
    const bool is_ours = !scope.is_shared && scope.shots.contains(shot);
    if (!is_ours) {
      continue;
    }
    // A change that spans shots can only come off the top.
    const bool is_apart_ok = i + 1 == steps.size() || scope.shots.size() == 1;
    return is_apart_ok ? std::optional<size_t>(i) : std::nullopt;
  }
  return std::nullopt;
}

bool HistoryManager::CanUndoIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  return !is_scope_open_ && StepFor(undo_, shot).has_value();
}

bool HistoryManager::CanRedoIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  return !is_scope_open_ && StepFor(redo_, shot).has_value();
}

QString HistoryManager::UndoLabelIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const std::optional<size_t> at = StepFor(undo_, shot);
  const bool can_undo = !is_scope_open_ && at.has_value();
  return can_undo ? undo_[*at].label : QString();
}

QString HistoryManager::RedoLabelIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const std::optional<size_t> at = StepFor(redo_, shot);
  const bool can_redo = !is_scope_open_ && at.has_value();
  return can_redo ? redo_[*at].label : QString();
}

QString HistoryManager::WhyNoUndoIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  if (is_scope_open_) {
    return tr("Finish the drag first");
  }
  const bool is_empty = undo_.empty();
  if (is_empty) {
    return tr("Nothing to undo");
  }
  return StepFor(undo_, shot).has_value()
             ? QString()
             : tr("Nothing to undo in this shot");
}

QString HistoryManager::WhyNoRedoIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  if (is_scope_open_) {
    return tr("Finish the drag first");
  }
  const bool is_empty = redo_.empty();
  if (is_empty) {
    return tr("Nothing to redo");
  }
  return StepFor(redo_, shot).has_value()
             ? QString()
             : tr("Nothing to redo in this shot");
}

void HistoryManager::TravelApart(std::deque<Step>* from,
                                 std::deque<Step>* to, size_t index,
                                 bool is_undo) {
  assert(from != nullptr && to != nullptr);
  assert(index < from->size());
  Step step = std::move((*from)[index]);
  from->erase(from->begin() + static_cast<std::ptrdiff_t>(index));
  // Only the step's own shots change; the others stay as they are.
  Project next = WithScopeFrom(current_, step.project, step.scope);
  // The steps that were made (or undone) after this one remember its
  // shots as they were; they take them as they are now. After an undo,
  // so do the states waiting to be redone (they all had the change in).
  for (size_t i = index; i < from->size(); ++i) {
    (*from)[i].project =
        WithScopeFrom(std::move((*from)[i].project), step.project, step.scope);
  }
  if (is_undo) {
    for (Step& other : *to) {
      other.project = WithScopeFrom(std::move(other.project), step.project,
                                    step.scope);
    }
  }
  to->push_back({step.label, std::move(current_), revision_, step.scope});
  current_ = std::move(next);
  revision_ = next_revision_++;
  emit Changed();
}

void HistoryManager::UndoIn(ShotId shot) {
  assert(shot.value() >= 0);
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const std::optional<size_t> at = StepFor(undo_, shot);
  const bool can_undo = !is_scope_open_ && at.has_value();
  if (!can_undo) {
    return;
  }
  const bool is_newest = *at + 1 == undo_.size();
  if (is_newest) {
    Travel(&undo_, &redo_);
  } else {
    TravelApart(&undo_, &redo_, *at, true);
  }
}

void HistoryManager::RedoIn(ShotId shot) {
  assert(shot.value() >= 0);
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const std::optional<size_t> at = StepFor(redo_, shot);
  const bool can_redo = !is_scope_open_ && at.has_value();
  if (!can_redo) {
    return;
  }
  const bool is_newest = *at + 1 == redo_.size();
  if (is_newest) {
    Travel(&redo_, &undo_);
  } else {
    TravelApart(&redo_, &undo_, *at, false);
  }
}

}  // namespace snapper
