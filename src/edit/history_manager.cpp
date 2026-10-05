#include "edit/history_manager.h"

#include <cassert>
#include <utility>

namespace snapper {

HistoryManager::HistoryManager(Project start) : current_(std::move(start)) {
  assert(undo_.empty());
  assert(!IsDirty());
}

void HistoryManager::Commit(const QString& label, Project next) {
  assert(!label.isEmpty());
  assert(!is_scope_open_);
  const bool is_unchanged = next == current_;
  const bool is_blocked = is_unchanged || is_scope_open_;
  if (is_blocked) {
    return;
  }
  Project before = std::exchange(current_, std::move(next));
  Record(label, std::move(before), revision_);
  revision_ = next_revision_++;
  emit Changed();
}

Result<void> HistoryManager::Apply(const QString& label,
                                   Result<Project> next) {
  assert(!label.isEmpty());
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  if (!next) {
    return std::unexpected(next.error());
  }
  if (is_scope_open_) {
    PreviewScope(std::move(*next));
  } else {
    Commit(label, std::move(*next));
  }
  return {};
}

void HistoryManager::Undo() {
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const bool can_undo = CanUndo();
  if (can_undo) {
    Travel(&undo_, &redo_);
  }
}

void HistoryManager::Redo() {
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  const bool can_redo = CanRedo();
  if (can_redo) {
    Travel(&redo_, &undo_);
  }
}

void HistoryManager::Reset(Project start) {
  assert(!is_scope_open_);
  current_ = std::move(start);
  undo_.clear();
  redo_.clear();
  revision_ = next_revision_++;
  saved_revision_ = revision_;
  assert(!IsDirty());
  emit Changed();
}

bool HistoryManager::CanUndo() const {
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  return !is_scope_open_ && !undo_.empty();
}

bool HistoryManager::CanRedo() const {
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  return !is_scope_open_ && !redo_.empty();
}

QString HistoryManager::UndoLabel() const {
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  const bool can_undo = CanUndo();
  return can_undo ? undo_.back().label : QString();
}

QString HistoryManager::RedoLabel() const {
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  const bool can_redo = CanRedo();
  return can_redo ? redo_.back().label : QString();
}

QString HistoryManager::WhyNoUndo() const {
  assert(undo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  const bool is_empty = undo_.empty();
  if (is_scope_open_) {
    return tr("Finish the drag first");
  }
  return is_empty ? tr("Nothing to undo") : QString();
}

QString HistoryManager::WhyNoRedo() const {
  assert(redo_.size() <= static_cast<size_t>(kMaxUndoSteps));
  assert(revision_ < next_revision_);
  const bool is_empty = redo_.empty();
  if (is_scope_open_) {
    return tr("Finish the drag first");
  }
  return is_empty ? tr("Nothing to redo") : QString();
}

void HistoryManager::MarkSaved() {
  assert(!is_scope_open_);
  assert(revision_ < next_revision_);
  saved_revision_ = revision_;
  emit Changed();
}

void HistoryManager::OpenScope() {
  assert(!is_scope_open_);
  assert(revision_ < next_revision_);
  is_scope_open_ = true;
  scope_start_ = current_;
  scope_start_revision_ = revision_;
  emit Changed();
}

void HistoryManager::PreviewScope(Project next) {
  assert(is_scope_open_);
  assert(revision_ < next_revision_);
  current_ = std::move(next);
  // The first preview is a new state, so the title shows it unsaved.
  const bool is_first_preview = revision_ == scope_start_revision_;
  if (is_first_preview) {
    revision_ = next_revision_++;
  }
  emit Changed();
}

void HistoryManager::CloseScope(const QString& label, bool keep) {
  assert(is_scope_open_);
  assert(!label.isEmpty());
  is_scope_open_ = false;
  const bool is_kept_change = keep && current_ != scope_start_;
  if (is_kept_change) {
    Record(label, std::move(scope_start_), scope_start_revision_);
  } else {
    current_ = std::move(scope_start_);
    revision_ = scope_start_revision_;
  }
  scope_start_ = Project();
  emit Changed();
}

void HistoryManager::Record(const QString& label, Project before,
                            int before_revision) {
  assert(!label.isEmpty());
  assert(before_revision < next_revision_);
  undo_.push_back({label, std::move(before), before_revision});
  const bool is_full = undo_.size() > static_cast<size_t>(kMaxUndoSteps);
  if (is_full) {
    undo_.pop_front();
  }
  redo_.clear();
}

void HistoryManager::Travel(std::deque<Step>* from, std::deque<Step>* to) {
  assert(from != nullptr && to != nullptr);
  assert(!from->empty());
  Step step = std::move(from->back());
  from->pop_back();
  to->push_back({step.label, std::move(current_), revision_});
  current_ = std::move(step.project);
  revision_ = step.revision;
  emit Changed();
}

}  // namespace snapper
