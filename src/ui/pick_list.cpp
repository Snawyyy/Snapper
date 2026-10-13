#include "ui/pick_list.h"

#include <QApplication>
#include <QItemSelection>
#include <QKeySequence>

#include <cassert>
#include <memory>
#include <optional>

#include "ui/pick_keys.h"

namespace snapper {
namespace {

// The keys that move the current item, so the pick follows them.
bool IsMoveKey(int key) {
  assert(key >= 0);
  switch (key) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
      return true;
    default:
      return false;
  }
}

bool IsDeleteKey(const QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_plain = (event->modifiers() & ~Qt::KeypadModifier) ==
                        Qt::NoModifier;
  return is_plain && (event->key() == Qt::Key_Delete ||
                      event->key() == Qt::Key_Backspace);
}

QItemSelectionModel::SelectionFlags FlagsOf(PickMode mode) {
  switch (mode) {
    case PickMode::kReplace:
      return QItemSelectionModel::ClearAndSelect;
    case PickMode::kAdd:
      return QItemSelectionModel::Select;
    case PickMode::kToggle:
      return QItemSelectionModel::Toggle;
    case PickMode::kRemove:
      return QItemSelectionModel::Deselect;
  }
  return QItemSelectionModel::NoUpdate;
}

}  // namespace

PickList* PickList::Install(QAbstractItemView* view) {
  assert(view != nullptr);
  assert(view->selectionModel() != nullptr);
  auto filter = std::unique_ptr<PickList>(new PickList(view));
  view->setSelectionMode(QAbstractItemView::NoSelection);
  view->installEventFilter(filter.get());
  view->viewport()->installEventFilter(filter.get());
  connect(view->selectionModel(), &QItemSelectionModel::currentChanged,
          filter.get(),
          [list = filter.get()](const QModelIndex& current) {
            list->FollowCurrent(current);
          });
  return filter.release();
}

PickList::PickList(QAbstractItemView* view) : QObject(view), view_(view) {
  assert(view_ != nullptr);
  assert(view_->viewport() != nullptr);
}

bool PickList::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(view_ != nullptr);
  const bool is_viewport = watched == view_->viewport();
  const QEvent::Type type = event->type();
  if (is_viewport && type == QEvent::MouseButtonPress) {
    Press(static_cast<const QMouseEvent*>(event));
  } else if (is_viewport && type == QEvent::MouseButtonRelease) {
    Release(static_cast<const QMouseEvent*>(event));
  } else if (watched == view_ && type == QEvent::ShortcutOverride) {
    // Claim the pick keys before a window shortcut takes them.
    const bool is_own = IsOwnKey(static_cast<const QKeyEvent*>(event));
    if (is_own) {
      event->accept();
      return true;
    }
  } else if (watched == view_ && type == QEvent::KeyPress) {
    return Key(static_cast<const QKeyEvent*>(event));
  }
  return QObject::eventFilter(watched, event);
}

void PickList::Press(const QMouseEvent* event) {
  assert(event != nullptr);
  assert(view_->selectionModel() != nullptr);
  is_keying_ = false;
  kept_ = QPersistentModelIndex();
  pressed_at_ = event->position().toPoint();
  const QModelIndex index = view_->indexAt(pressed_at_);
  const Qt::KeyboardModifiers mods = event->modifiers();
  const bool is_on_item = index.isValid();
  const bool is_picked =
      is_on_item && view_->selectionModel()->isSelected(index);
  const bool is_right = event->button() == Qt::RightButton;
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_on_item) {
    const bool is_plain = !mods.testFlag(Qt::ShiftModifier) &&
                          !mods.testFlag(Qt::ControlModifier);
    if (is_plain && (is_left || is_right)) {
      view_->selectionModel()->clearSelection();
    }
    return;
  }
  if (is_right) {
    if (!is_picked) {
      Apply(index, PickMode::kReplace);
    }
    return;
  }
  if (!is_left) {
    return;
  }
  const std::optional<PickMode> mode = ClickMode(mods, is_picked);
  if (mode.has_value()) {
    Apply(index, *mode);
  } else {
    kept_ = index;
  }
}

void PickList::Release(const QMouseEvent* event) {
  assert(event != nullptr);
  assert(view_ != nullptr);
  const bool is_kept = kept_.isValid() && event->button() == Qt::LeftButton;
  if (!is_kept) {
    return;
  }
  const QPoint at = event->position().toPoint();
  const bool is_still = (at - pressed_at_).manhattanLength() <
                        QApplication::startDragDistance();
  const bool is_same = view_->indexAt(at) == QModelIndex(kept_);
  if (is_still && is_same) {
    Apply(kept_, PickMode::kReplace);
  }
  kept_ = QPersistentModelIndex();
}

bool PickList::IsOwnKey(const QKeyEvent* event) const {
  assert(event != nullptr);
  assert(view_ != nullptr);
  const bool has_pick = view_->selectionModel()->hasSelection();
  const bool is_all = event->matches(QKeySequence::SelectAll);
  const bool is_escape = event->key() == Qt::Key_Escape && has_pick;
  const bool is_delete = IsDeleteKey(event) && has_pick;
  return is_all || is_escape || is_delete;
}

bool PickList::Key(const QKeyEvent* event) {
  assert(event != nullptr);
  assert(view_->selectionModel() != nullptr);
  const bool is_move = IsMoveKey(event->key());
  if (is_move) {
    // The view moves the current item; FollowCurrent picks it.
    is_keying_ = true;
    is_keying_shift_ = event->modifiers().testFlag(Qt::ShiftModifier);
    return false;
  }
  const bool is_own = IsOwnKey(event);
  if (!is_own) {
    return false;
  }
  if (event->matches(QKeySequence::SelectAll)) {
    PickAll();
  } else if (event->key() == Qt::Key_Escape) {
    view_->selectionModel()->clearSelection();
  } else {
    emit DeleteAsked();
  }
  return true;
}

void PickList::FollowCurrent(const QModelIndex& current) {
  assert(view_ != nullptr);
  assert(!is_keying_ || view_->selectionModel() != nullptr);
  const bool is_followed = is_keying_ && current.isValid();
  is_keying_ = false;
  if (is_followed) {
    Apply(current, is_keying_shift_ ? PickMode::kAdd : PickMode::kReplace);
  }
}

void PickList::Apply(const QModelIndex& index, PickMode mode) {
  assert(index.isValid());
  assert(view_->selectionModel() != nullptr);
  view_->selectionModel()->select(index, FlagsOf(mode) | Rows());
}

void PickList::PickAll() {
  assert(view_ != nullptr);
  const QAbstractItemModel* model = view_->model();
  const QModelIndex root = view_->rootIndex();
  const int rows = model == nullptr ? 0 : model->rowCount(root);
  const int columns = model == nullptr ? 0 : model->columnCount(root);
  if (rows == 0 || columns == 0) {
    return;
  }
  const QItemSelection all(model->index(0, 0, root),
                           model->index(rows - 1, columns - 1, root));
  view_->selectionModel()->select(all, QItemSelectionModel::ClearAndSelect);
}

QItemSelectionModel::SelectionFlags PickList::Rows() const {
  assert(view_ != nullptr);
  const bool is_rows =
      view_->selectionBehavior() == QAbstractItemView::SelectRows;
  return is_rows ? QItemSelectionModel::Rows
                 : QItemSelectionModel::NoUpdate;
}

}  // namespace snapper
