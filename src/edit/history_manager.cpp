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
  const bool is_folded = is_bursting_ && CanFoldBurst(label, revision_);
  Project before = std::exchange(current_, std::move(next));
  if (!is_folded) {
    Record(label, std::move(before), revision_);
  }
  revision_ = next_revision_++;
  if (is_bursting_) {
    NoteBurst(label);
  }
  emit Changed();
}

bool HistoryManager::CanFoldBurst(const QString& label, int revision) const {
  assert(revision_ < next_revision_);
  assert(!label.isEmpty());
  const bool is_same_burst = !undo_.empty() && revision == burst_revision_ &&
                             label == burst_label_ && redo_.empty();
  return is_same_burst && burst_clock_.isValid() &&
         burst_clock_.elapsed() < kBurstMs;
}

void HistoryManager::NoteBurst(const QString& label) {
  assert(!label.isEmpty());
  assert(revision_ < next_revision_);
  burst_label_ = label;
  burst_revision_ = revision_;
  burst_clock_.restart();
}

HistoryManager::BurstGuard::BurstGuard(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->is_bursting_);
  history_->is_bursting_ = true;
}

HistoryManager::BurstGuard::~BurstGuard() {
  assert(history_ != nullptr);
  assert(history_->is_bursting_);
  history_->is_bursting_ = false;
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

void HistoryManager::Reset(Project start, bool is_saved) {
  assert(!is_scope_open_);
  current_ = std::move(start);
  undo_.clear();
  redo_.clear();
  revision_ = next_revision_++;
  // Revisions start at 0, so -1 matches no state: dirty until saved.
  saved_revision_ = is_saved ? revision_ : -1;
  assert(IsDirty() != is_saved);
  emit Changed();
}

void HistoryManager::MapProjects(
    const std::function<Project(const Project&)>& map) {
  assert(!is_scope_open_);
  assert(map != nullptr);
  current_ = map(current_);
  for (Step& step : undo_) {
    step.project = map(step.project);
  }
  for (Step& step : redo_) {
    step.project = map(step.project);
  }
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
  // A live edit closing inside a burst (a wheel tick on a dial) folds
  // into the step before it instead of making a new one.
  const bool is_folded = is_kept_change && is_bursting_ &&
                         CanFoldBurst(label, scope_start_revision_);
  if (is_kept_change) {
    if (!is_folded) {
      Record(label, std::move(scope_start_), scope_start_revision_);
    }
    if (is_bursting_) {
      NoteBurst(label);
    }
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
  StepScope scope = ScopeOf(before, current_);
  undo_.push_back(
      {label, std::move(before), before_revision, std::move(scope)});
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
  to->push_back({step.label, std::move(current_), revision_, step.scope});
  current_ = std::move(step.project);
  revision_ = step.revision;
  emit Changed();
}

}  // namespace snapper
