#ifndef SNAPPER_UI_INSPECTOR_H_
#define SNAPPER_UI_INSPECTOR_H_

#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/managers.h"
#include "ui/motion_box.h"

namespace snapper {

// The right-hand panel: exact pose and camera numbers at the playhead,
// the picked layer's settings, motion loops and saved poses, stacked
// and scrollable.
class Inspector final : public QScrollArea {
  Q_OBJECT

 public:
  explicit Inspector(const Managers& managers);
  ~Inspector() override;

 signals:
  void Problem(const QString& why);

 private:
  // The body holds the boxes; it comes first so they leave it first.
  QWidget body_;
  QVBoxLayout layout_;
  PoseBox pose_;
  CameraBox camera_;
  LayerBox layer_;
  MotionBox motion_;
  PosesBox poses_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_H_
