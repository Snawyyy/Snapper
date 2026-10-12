#ifndef SNAPPER_UI_INLINE_TEXT_EDIT_H_
#define SNAPPER_UI_INLINE_TEXT_EDIT_H_

#include <QPlainTextEdit>
#include <QRect>
#include <QString>

#include "model/reel.h"

namespace snapper {

// Typing a text clip's words where they show (the video preview or the
// clip on the reel): Enter adds a line, Ctrl+Enter or clicking away
// finishes, Escape cancels. It only reports the words; the caller
// lands them through ReelManager as one step.
class InlineTextEdit final : public QPlainTextEdit {
  Q_OBJECT

 public:
  explicit InlineTextEdit(QWidget* parent);

  // Opens over where (parent pixels) holding text, all of it picked.
  void Open(ClipId clip, const QString& text, const QRect& where);
  bool is_open() const { return clip_.IsValid(); }

 signals:
  void Finished(ClipId clip, const QString& text);

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;

 private:
  void Close(bool is_kept);

  ClipId clip_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_INLINE_TEXT_EDIT_H_
