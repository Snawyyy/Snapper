#ifndef SNAPPER_UI_POINT_MOTION_BOX_H_
#define SNAPPER_UI_POINT_MOTION_BOX_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QString>

#include "model/doll.h"
#include "edit/rig_manager.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The warp dots picked on the stage: whether they move by themselves
// (a wave, a pulse...) and, when they do, their size, cycle, direction
// and delay. Every picked dot is edited at once, a number that differs
// shows Mixed. Greyed out with the reason when no dot is picked.
class PointMotionBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit PointMotionBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  void Build();
  // Gives the picked dots kind, keeping their other settings.
  void SetKind(WarpMotionKind kind);
  // Sends one number field to the motion of every picked dot that has
  // one.
  void Commit(MotionField field, double value);

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QComboBox kind_;
  QDoubleSpinBox size_;
  QSpinBox cycle_;
  QDoubleSpinBox angle_;
  QDoubleSpinBox delay_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POINT_MOTION_BOX_H_
