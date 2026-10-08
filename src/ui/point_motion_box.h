#ifndef SNAPPER_UI_POINT_MOTION_BOX_H_
#define SNAPPER_UI_POINT_MOTION_BOX_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QString>

#include "model/doll.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The warp dot picked on the stage: whether it moves by itself (a
// wave, a pulse...) and, when it does, its size, cycle, direction and
// delay. Greyed out with the reason when no dot is picked.
class PointMotionBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit PointMotionBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  void Build();
  // Gives the picked dot kind, keeping its other settings.
  void SetKind(WarpMotionKind kind);
  // Sends the number fields to the picked dot's motion.
  void Commit();

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
