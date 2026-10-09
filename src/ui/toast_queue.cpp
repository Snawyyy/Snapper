#include "ui/toast_queue.h"

#include <algorithm>
#include <cassert>

namespace snapper {

QString CountText(const QString& verb, int count, const QString& one,
                  const QString& many) {
  assert(count >= 0);
  assert(!verb.isEmpty());
  const bool is_single = count == 1;
  return QStringLiteral("%1 %2 %3")
      .arg(verb)
      .arg(count)
      .arg(is_single ? one : many);
}

int ToastQueue::Add(Toast toast) {
  assert(toast.id > 0);
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  const bool is_full = static_cast<int>(toasts_.size()) == kMaxToasts;
  if (is_full) {
    toasts_.erase(toasts_.begin());
  }
  const int id = toast.id;
  toasts_.push_back(std::move(toast));
  return id;
}

int ToastQueue::Push(const QString& text) {
  assert(!text.isEmpty());
  assert(next_id_ > 0);
  for (Toast& shown : toasts_) {
    const bool is_repeat = !shown.has_undo && shown.text == text;
    if (is_repeat) {
      shown.left_ms = kToastMs;
      return shown.id;
    }
  }
  Toast toast;
  toast.id = next_id_++;
  toast.text = text;
  return Add(std::move(toast));
}

int ToastQueue::PushUndoable(const QString& text, int step,
                             const QString& undo_label) {
  assert(!text.isEmpty());
  assert(next_id_ > 0);
  Toast toast;
  toast.id = next_id_++;
  toast.text = text;
  toast.has_undo = true;
  toast.step = step;
  toast.undo_label = undo_label;
  return Add(std::move(toast));
}

bool ToastQueue::Tick(int elapsed_ms) {
  assert(elapsed_ms >= 0);
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  for (Toast& shown : toasts_) {
    shown.left_ms -= elapsed_ms;
  }
  const auto is_over = [](const Toast& shown) { return shown.left_ms <= 0; };
  const auto end = std::remove_if(toasts_.begin(), toasts_.end(), is_over);
  const bool any_over = end != toasts_.end();
  toasts_.erase(end, toasts_.end());
  return any_over;
}

bool ToastQueue::Dismiss(int id) {
  assert(id >= 0);
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  const auto is_it = [id](const Toast& shown) { return shown.id == id; };
  const auto it = std::find_if(toasts_.begin(), toasts_.end(), is_it);
  const bool is_found = it != toasts_.end();
  if (is_found) {
    toasts_.erase(it);
  }
  return is_found;
}

bool ToastQueue::DismissNewest() {
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  assert(next_id_ > 0);
  const bool is_empty = toasts_.empty();
  if (is_empty) {
    return false;
  }
  toasts_.pop_back();
  return true;
}

bool ToastQueue::Retire(int current_step) {
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  assert(next_id_ > 0);
  bool any_retired = false;
  for (Toast& shown : toasts_) {
    const bool is_stale = shown.has_undo && shown.step != current_step;
    if (is_stale) {
      shown.has_undo = false;
      any_retired = true;
    }
  }
  return any_retired;
}

bool ToastQueue::TakeUndo(int id, int current_step) {
  assert(id >= 0);
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  const Toast* shown = Find(id);
  const bool is_offered =
      shown != nullptr && shown->has_undo && shown->step == current_step;
  if (!is_offered) {
    return false;
  }
  return Dismiss(id);
}

const Toast* ToastQueue::Find(int id) const {
  assert(id >= 0);
  assert(static_cast<int>(toasts_.size()) <= kMaxToasts);
  for (const Toast& shown : toasts_) {
    const bool is_it = shown.id == id;
    if (is_it) {
      return &shown;
    }
  }
  return nullptr;
}

}  // namespace snapper
