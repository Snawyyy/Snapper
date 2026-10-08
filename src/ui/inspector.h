#ifndef SNAPPER_UI_INSPECTOR_H_
#define SNAPPER_UI_INSPECTOR_H_

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/drag_box.h"
#include "ui/shot_box.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/managers.h"
#include "ui/motion_box.h"
#include "ui/point_motion_box.h"

namespace snapper {

// The right-hand panel: only the settings of what is picked. With
// nothing picked on the stage: the shot and its camera. With pieces or
// layers: their pose at the playhead and motion loops, the layer's
// settings, saved poses for a doll, and the picked warp dot's drag
// and animation.
// Stacked and scrollable.
class Inspector final : public QScrollArea {
  Q_OBJECT

 public:
  explicit Inspector(const Managers& managers);
  ~Inspector() override;

  // Shows the boxes for what is picked now.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The body holds the boxes; it comes first so they leave it first.
  QWidget body_;
  QVBoxLayout layout_;
  Managers managers_;
  QLabel hint_;
  ShotBox shot_;
  PoseBox pose_;
  DragBox drag_;
  PointMotionBox point_motion_;
  CameraBox camera_;
  LayerBox layer_;
  MotionBox motion_;
  PosesBox poses_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_H_
