#ifndef SNAPPER_UI_RESET_ON_DOUBLE_CLICK_H_
#define SNAPPER_UI_RESET_ON_DOUBLE_CLICK_H_

#include <QDoubleSpinBox>
#include <QObject>

#include "ui/bound_drag.h"

class QEvent;

namespace snapper {

class LiveEdit;

// A double click on a number field sets it to its neutral value as one
// undo step (the way a dial or fader resets), unless the field is
// greyed out. Lives as a child of the field, which owns it.
class ResetOnDoubleClick final : public QObject {
  Q_OBJECT

 public:
  // Gives box the reset to neutral; live is the field's live edit.
  static void Install(QDoubleSpinBox* box, double neutral, LiveEdit* live);

  ResetOnDoubleClick(QDoubleSpinBox* box, double neutral, LiveEdit* live);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  BoundDrag drag_;
  double neutral_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RESET_ON_DOUBLE_CLICK_H_
