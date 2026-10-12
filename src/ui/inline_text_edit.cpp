#include "ui/inline_text_edit.h"

#include <QKeyEvent>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

constexpr int kSmallest = 120;

}  // namespace

InlineTextEdit::InlineTextEdit(QWidget* parent) : QPlainTextEdit(parent) {
  assert(parent != nullptr);
  setToolTip(tr("Enter adds a line, Ctrl+Enter finishes, Escape cancels."));
  setTabChangesFocus(true);
  hide();
  assert(!clip_.IsValid());
}

void InlineTextEdit::Open(ClipId clip, const QString& text,
                          const QRect& where) {
  assert(clip.IsValid());
  assert(parentWidget() != nullptr);
  clip_ = clip;
  setPlainText(text);
  selectAll();
  const QRect room = parentWidget()->rect();
  QRect box = where.adjusted(-8, -8, 8, 8);
  box.setWidth(std::max(box.width(), kSmallest));
  box.setHeight(std::max(box.height(), fontMetrics().height() * 3));
  box.moveLeft(std::clamp(box.left(), 0, std::max(0, room.width() - box.width())));
  box.moveTop(std::clamp(box.top(), 0, std::max(0, room.height() - box.height())));
  setGeometry(box);
  show();
  raise();
  setFocus();
}

void InlineTextEdit::Close(bool is_kept) {
  assert(clip_.IsValid() || !isVisible());
  const ClipId clip = clip_;
  const QString text = toPlainText();
  // Cleared first, so the focus leaving as it hides closes nothing twice.
  clip_ = ClipId();
  hide();
  if (is_kept && clip.IsValid()) {
    emit Finished(clip, text);
  }
  assert(!clip_.IsValid());
}

void InlineTextEdit::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_enter =
      event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
  const bool is_finish =
      is_enter && event->modifiers().testFlag(Qt::ControlModifier);
  const bool is_cancel = event->key() == Qt::Key_Escape;
  if (is_finish || is_cancel) {
    event->accept();
    Close(is_finish);
    return;
  }
  QPlainTextEdit::keyPressEvent(event);
}

void InlineTextEdit::focusOutEvent(QFocusEvent* event) {
  assert(event != nullptr);
  QPlainTextEdit::focusOutEvent(event);
  // Clicking away keeps what was typed, as Ctrl+Enter does.
  if (is_open()) {
    Close(true);
  }
}

}  // namespace snapper
