#ifndef SNAPPER_UI_TOAST_QUEUE_H_
#define SNAPPER_UI_TOAST_QUEUE_H_

#include <QString>

#include <vector>

namespace snapper {

// How long a toast stays, and how many show at once.
constexpr int kToastMs = 4000;
constexpr int kMaxToasts = 3;

// One message on screen. A toast with an Undo button is tied to the
// history step it announces (step), so the button can only undo that
// step.
struct Toast final {
  int id = 0;
  QString text;
  bool has_undo = false;
  // The history revision the toast announces; used only with has_undo.
  int step = 0;
  // Names the step on the Undo button's tooltip.
  QString undo_label;
  int left_ms = kToastMs;
};

// "Deleted 1 key" or "Deleted 3 keys": the noun follows the count.
QString CountText(const QString& verb, int count, const QString& one,
                  const QString& many);

// The pure part of the toasts: order, stacking, timing and dismissal.
// No widget and no clock; the caller says how much time passed.
class ToastQueue final {
 public:
  // Adds a toast and returns its id. The same text again with no Undo
  // refreshes the one already there instead of stacking. When kMaxToasts
  // are showing, the oldest goes.
  int Push(const QString& text);
  // A toast that offers Undo for the history step step.
  int PushUndoable(const QString& text, int step,
                   const QString& undo_label);

  // Time passes; expired toasts go. True when anything went.
  bool Tick(int elapsed_ms);
  // True when a toast with that id was showing.
  bool Dismiss(int id);
  // Takes the newest toast away; false when none.
  bool DismissNewest();

  // Toasts whose step is no longer the current history revision lose
  // their Undo button (the project moved on). True when any did.
  bool Retire(int current_step);
  // The Undo button was pressed: true, and the toast is dismissed, when
  // it still offers Undo for exactly current_step.
  bool TakeUndo(int id, int current_step);

  // Oldest first.
  const std::vector<Toast>& toasts() const { return toasts_; }
  const Toast* Find(int id) const;
  bool empty() const { return toasts_.empty(); }

 private:
  int Add(Toast toast);

  std::vector<Toast> toasts_;
  int next_id_ = 1;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOAST_QUEUE_H_
