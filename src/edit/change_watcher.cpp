#include "edit/change_watcher.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "edit/history_manager.h"

namespace snapper {

ChangeWatcher::ChangeWatcher(HistoryManager* history, QObject* parent)
    : QObject(parent),
      history_(history),
      seen_(history->current()),
      newest_revision_(history->revision()) {
  assert(history_ != nullptr);
  assert(parent != nullptr);
  connect(history_, &HistoryManager::Changed, this,
          &ChangeWatcher::OnChanged);
}

void ChangeWatcher::OnChanged() {
  assert(history_ != nullptr);
  assert(newest_revision_ >= 0);
  // A drag lands as one step, so its live previews are not heard.
  const bool is_dragging = history_->IsScopeOpen();
  if (is_dragging) {
    return;
  }
  // Neither undo nor redo is possible only on a fresh start (a file
  // was opened): that is no edit to show.
  const bool is_fresh_start = !history_->CanUndo() && !history_->CanRedo();
  const bool is_same = history_->current() == seen_;
  const bool is_travel = history_->revision() <= newest_revision_;
  newest_revision_ = std::max(newest_revision_, history_->revision());
  const bool is_silent = is_fresh_start || is_same;
  if (is_silent) {
    seen_ = history_->current();
    return;
  }
  Project before = std::exchange(seen_, history_->current());
  emit Stepped(before, is_travel);
}

}  // namespace snapper
