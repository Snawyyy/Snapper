#ifndef SNAPPER_UI_TOAST_MANAGER_H_
#define SNAPPER_UI_TOAST_MANAGER_H_

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "ui/toast_queue.h"

namespace snapper {

class HistoryManager;

// Owns the toasts: what is showing, for how long, in what order, and
// how each goes away. Widgets only call Problem, Done or Finish; the
// ToastView draws what this holds. A toast that offers Undo is tied to
// the history step it announces, and loses the button once the project
// moves on.
class ToastManager final : public QObject {
  Q_OBJECT

 public:
  explicit ToastManager(HistoryManager* history);

  // Something the user tried could not be done. Empty text is ignored,
  // so ProblemOf can be passed straight in.
  void Problem(const QString& why);
  // A big action just landed as the newest history step: a toast with
  // an Undo button that undoes exactly that step.
  void Done(const QString& text);
  // Something not undoable happened and went well (a file saved): a
  // plain toast with no Undo.
  void Say(const QString& text);
  // The same for a big win (an export done, a rig saved): the toast, and
  // the window bursts (see SuccessBurst).
  void Cheer(const QString& text);
  // Passes on the outcome of an edit: a problem is shown as one, no
  // problem shows done_text with Undo.
  void Finish(const QString& problem, const QString& done_text);

  // The Undo button of a toast. Does nothing unless the toast still
  // offers Undo for the current step.
  void Undo(int id);
  void Dismiss(int id);
  // Escape: takes the newest toast away; false when none showed.
  bool DismissNewest();

  // Lets time pass; the timer calls this, a test can too.
  void Advance(int elapsed_ms);

  const ToastQueue& queue() const { return queue_; }

 signals:
  // A toast came, went, lost its Undo or ran down.
  void Changed();
  // Something big went well.
  void Cheered();

 private:
  void OnTick();
  void RunClockWhileShowing();

  HistoryManager* history_;
  ToastQueue queue_;
  QTimer clock_;
  QElapsedTimer since_tick_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOAST_MANAGER_H_
