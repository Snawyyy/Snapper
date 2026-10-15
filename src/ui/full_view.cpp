#include "ui/full_view.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QSplitterHandle>
#include <QStatusBar>
#include <QToolBar>

#include <cassert>

namespace snapper {
namespace {

// The deepest a view sits under the window, and the most widgets one
// level holds, as loop bounds.
constexpr int kMaxDepth = 64;
constexpr int kMaxSiblings = 512;

}  // namespace

FullView::FullView(QMainWindow* window) : window_(window) {
  assert(window_ != nullptr);
  assert(hidden_.empty());
}

FullView::~FullView() { Leave(); }

void FullView::Hide(QWidget* widget) {
  assert(widget != nullptr);
  assert(hidden_.size() < 10000);
  const bool is_shown = widget->isVisible();
  if (is_shown) {
    widget->hide();
    hidden_.emplace_back(widget);
  }
}

void FullView::Enter(QWidget* view) {
  assert(view != nullptr);
  assert(window_->isAncestorOf(view));
  if (is_on_) {
    return;
  }
  is_on_ = true;
  // Up from the view, everything beside it at each level steps aside.
  QWidget* at = view;
  for (int depth = 0; depth < kMaxDepth && at != window_; ++depth) {
    QWidget* parent = at->parentWidget();
    const bool has_parent = parent != nullptr;
    if (!has_parent) {
      break;
    }
    const QObjectList& kids = parent->children();
    for (int i = 0; i < kids.size() && i < kMaxSiblings; ++i) {
      auto* sibling = qobject_cast<QWidget*>(kids[i]);
      // Splitter handles go with what they part; windows of their own
      // (menus, dialogs) are not part of the page.
      const bool is_beside = sibling != nullptr && sibling != at &&
                             !sibling->isWindow() &&
                             qobject_cast<QSplitterHandle*>(sibling) ==
                                 nullptr;
      if (is_beside) {
        Hide(sibling);
      }
    }
    at = parent;
  }
  for (QDockWidget* dock : window_->findChildren<QDockWidget*>()) {
    Hide(dock);
  }
  for (QToolBar* bar : window_->findChildren<QToolBar*>()) {
    Hide(bar);
  }
  Hide(window_->menuBar());
  Hide(window_->statusBar());
  was_full_screen_ = window_->isFullScreen();
  was_maximized_ = window_->isMaximized();
  window_->showFullScreen();
}

void FullView::Leave() {
  assert(window_ != nullptr);
  assert(hidden_.size() < 10000);
  if (!is_on_) {
    return;
  }
  is_on_ = false;
  for (const QPointer<QWidget>& widget : hidden_) {
    const bool is_alive = !widget.isNull();
    if (is_alive) {
      widget->show();
    }
  }
  hidden_.clear();
  const bool stays_full = was_full_screen_;
  if (stays_full) {
    return;
  }
  if (was_maximized_) {
    window_->showMaximized();
  } else {
    window_->showNormal();
  }
}

}  // namespace snapper
