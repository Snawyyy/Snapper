#ifndef SNAPPER_UI_BOUND_DRAG_H_
#define SNAPPER_UI_BOUND_DRAG_H_

#include <QDoubleSpinBox>

#include <memory>

#include <vector>

#include "edit/history_manager.h"

namespace snapper {

class LiveEdit;

// What the hand-made controls (fader, pad) share: they work number
// fields. A drag sets the fields, which are the project's edit path
// (MakeLive), so the whole drag is one undo step; Cancel puts the
// numbers back and drops that step. live may be null for a field that
// edits no project.
class BoundDrag final {
 public:
  BoundDrag(std::vector<QDoubleSpinBox*> boxes, LiveEdit* live);

  bool is_active() const { return is_active_; }
  size_t count() const { return boxes_.size(); }
  double value(size_t index) const;
  // The number each field had when the drag began.
  double start(size_t index) const;
  // The range a field allows.
  double low(size_t index) const;
  double high(size_t index) const;
  bool is_enabled() const;

  void Begin();
  // Sets one field, kept inside its range.
  void Set(size_t index, double value);
  // Lands the drag as one undo step.
  void Finish();
  // Puts the numbers back and drops the step.
  void Cancel();
  // A burst for wheel ticks (see LiveEdit::Burst); nothing without a
  // live edit.
  std::unique_ptr<HistoryManager::BurstGuard> Burst();
  // One step that sets every field to values (a reset).
  void SetAll(const std::vector<double>& values);

 private:
  std::vector<QDoubleSpinBox*> boxes_;
  std::vector<double> starts_;
  LiveEdit* live_;
  bool is_active_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_BOUND_DRAG_H_
