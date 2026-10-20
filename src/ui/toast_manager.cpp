#include "ui/toast_manager.h"

#include <cassert>

#include "edit/history_manager.h"

namespace snapper {
namespace {

// Often enough for the countdown bar to look smooth.
constexpr int kTickMs = 40;

}  // namespace

ToastManager::ToastManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  clock_.setInterval(kTickMs);
  connect(&clock_, &QTimer::timeout, this, &ToastManager::OnTick);
  // The project moved on: an old toast must not undo a newer step.
  connect(history_, &HistoryManager::Changed, this, [this] {
    const bool is_retired = queue_.Retire(history_->revision());
    if (is_retired) {
      emit Changed();
    }
  });
  assert(queue_.empty());
}

void ToastManager::Problem(const QString& why) {
  assert(why.size() < 100000);
  assert(history_ != nullptr);
  const bool is_problem = !why.isEmpty();
  if (!is_problem) {
    return;
  }
  queue_.Push(why, ToastKind::kProblem);
  RunClockWhileShowing();
  emit Changed();
}

void ToastManager::Say(const QString& text) {
  assert(!text.isEmpty());
  assert(history_ != nullptr);
  queue_.Push(text);
  RunClockWhileShowing();
  emit Changed();
}

void ToastManager::Cheer(const QString& text) {
  assert(!text.isEmpty());
  assert(history_ != nullptr);
  queue_.Push(text, ToastKind::kCheer);
  RunClockWhileShowing();
  emit Changed();
  emit Cheered();
}

void ToastManager::Done(const QString& text) {
  assert(!text.isEmpty());
  assert(history_ != nullptr);
  const bool can_undo = history_->CanUndo();
  if (can_undo) {
    queue_.PushUndoable(text, history_->revision(), history_->UndoLabel());
  } else {
    queue_.Push(text);
  }
  RunClockWhileShowing();
  emit Changed();
}

void ToastManager::Finish(const QString& problem, const QString& done_text) {
  assert(!done_text.isEmpty());
  assert(problem.size() < 100000);
  const bool is_problem = !problem.isEmpty();
  if (is_problem) {
    Problem(problem);
    return;
  }
  Done(done_text);
}

void ToastManager::Undo(int id) {
  assert(id >= 0);
  assert(history_ != nullptr);
  const bool is_offered = queue_.TakeUndo(id, history_->revision());
  if (!is_offered) {
    return;
  }
  // Dismissed first: the undo's own Changed has nothing left to retire.
  emit Changed();
  history_->Undo();
}

void ToastManager::Dismiss(int id) {
  assert(id >= 0);
  assert(history_ != nullptr);
  const bool is_dismissed = queue_.Dismiss(id);
  if (is_dismissed) {
    emit Changed();
  }
}

bool ToastManager::DismissNewest() {
  assert(history_ != nullptr);
  assert(kTickMs > 0);
  const bool is_dismissed = queue_.DismissNewest();
  if (is_dismissed) {
    emit Changed();
  }
  return is_dismissed;
}

void ToastManager::Advance(int elapsed_ms) {
  assert(elapsed_ms >= 0);
  assert(history_ != nullptr);
  queue_.Tick(elapsed_ms);
  emit Changed();
}

void ToastManager::OnTick() {
  assert(since_tick_.isValid());
  assert(clock_.isActive());
  const int elapsed = static_cast<int>(since_tick_.restart());
  Advance(elapsed);
  const bool is_idle = queue_.empty();
  if (is_idle) {
    clock_.stop();
  }
}

void ToastManager::RunClockWhileShowing() {
  assert(!queue_.empty());
  assert(clock_.interval() == kTickMs);
  const bool is_stopped = !clock_.isActive();
  if (is_stopped) {
    since_tick_.start();
    clock_.start();
  }
}

}  // namespace snapper
