#include "ui/inspector.h"

#include <QScrollBar>

#include <cassert>

namespace snapper {

Inspector::Inspector(const Managers& managers)
    : layout_(&body_),
      pose_(managers),
      camera_(managers),
      layer_(managers),
      motion_(managers),
      poses_(managers) {
  assert(managers.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(4);
  layout_.addWidget(&pose_);
  layout_.addWidget(&layer_);
  layout_.addWidget(&camera_);
  layout_.addWidget(&motion_);
  layout_.addWidget(&poses_);
  layout_.addStretch(1);
  connect(&pose_, &PoseBox::Problem, this, &Inspector::Problem);
  connect(&camera_, &CameraBox::Problem, this, &Inspector::Problem);
  connect(&layer_, &LayerBox::Problem, this, &Inspector::Problem);
  connect(&motion_, &MotionBox::Problem, this, &Inspector::Problem);
  connect(&poses_, &PosesBox::Problem, this, &Inspector::Problem);
  setWidgetResizable(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // The scroll area must not delete body_, a member; it is taken back in
  // the destructor below.
  setWidget(&body_);
  // Wide enough that paired fields (x and y) never get cut off.
  setMinimumWidth(body_.minimumSizeHint().width() +
                  verticalScrollBar()->sizeHint().width() + 4);
  assert(widget() == &body_);
}

Inspector::~Inspector() {
  assert(widget() == &body_);
  QWidget* body = takeWidget();
  assert(body == &body_);
}

}  // namespace snapper
