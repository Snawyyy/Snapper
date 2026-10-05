#ifndef SNAPPER_UI_KEYED_BOXES_H_
#define SNAPPER_UI_KEYED_BOXES_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>

#include <array>

#include "ui/managers.h"

namespace snapper {

// Exact numbers for the picked piece or layer at the playhead. Typing a
// value keys it there, like dragging on the stage does.
class PoseBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit PoseBox(const Managers& managers);
  // Shows the picked track's pose at the playhead.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  enum Field { kTurn, kX, kY, kScaleX, kScaleY, kSkew, kOpacity, kCount };

  void Commit();
  void SwapDrawing(int index);

  Managers managers_;
  QFormLayout layout_;
  std::array<QDoubleSpinBox, kCount> fields_;
  QComboBox drawing_;
};

// The camera of the shot under the playhead, keyed at the playhead.
class CameraBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit CameraBox(const Managers& managers);
  // Shows the camera at the playhead.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  enum Field { kX, kY, kZoom, kTurn, kShake, kCount };

  void Commit();

  Managers managers_;
  QFormLayout layout_;
  std::array<QDoubleSpinBox, kCount> fields_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KEYED_BOXES_H_
