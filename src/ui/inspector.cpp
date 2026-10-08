#include "ui/inspector.h"

#include <QScrollBar>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"

namespace snapper {

Inspector::Inspector(const Managers& managers)
    : layout_(&body_),
      managers_(managers),
      shot_(managers),
      pose_(managers),
      drag_(managers),
      camera_(managers),
      layer_(managers),
      motion_(managers),
      link_(managers),
      poses_(managers) {
  assert(managers.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(4);
  hint_.setWordWrap(true);
  hint_.setText(tr("Add a shot in the strip above to start."));
  layout_.addWidget(&hint_);
  layout_.addWidget(&shot_);
  layout_.addWidget(&pose_);
  layout_.addWidget(&drag_);
  layout_.addWidget(&layer_);
  layout_.addWidget(&camera_);
  layout_.addWidget(&motion_);
  layout_.addWidget(&link_);
  layout_.addWidget(&poses_);
  layout_.addStretch(1);
  connect(&pose_, &PoseBox::Problem, this, &Inspector::Problem);
  connect(&shot_, &ShotBox::Problem, this, &Inspector::Problem);
  connect(&drag_, &DragBox::Problem, this, &Inspector::Problem);
  connect(&camera_, &CameraBox::Problem, this, &Inspector::Problem);
  connect(&layer_, &LayerBox::Problem, this, &Inspector::Problem);
  connect(&motion_, &MotionBox::Problem, this, &Inspector::Problem);
  connect(&link_, &LinkBox::Problem, this, &Inspector::Problem);
  connect(&poses_, &PosesBox::Problem, this, &Inspector::Problem);
  setWidgetResizable(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // The scroll area must not delete body_, a member; it is taken back in
  // the destructor below.
  setWidget(&body_);
  // Wide enough that paired fields (x and y) never get cut off.
  setMinimumWidth(body_.minimumSizeHint().width() +
                  verticalScrollBar()->sizeHint().width() + 4);
  Follow(managers_, this);
  Refresh();
  assert(widget() == &body_);
}

void Inspector::Refresh() {
  const SelectionManager& selection = *managers_.selection;
  const Project& project = managers_.history->current();
  const bool has_shot = FindShot(project, selection.shot()) != nullptr;
  const bool has_pick = has_shot && !selection.picks().empty();
  const bool is_doll =
      has_pick && DollOfLayer(project, selection.shot(), selection.layer()) !=
                      nullptr;
  hint_.setVisible(!has_shot);
  shot_.setVisible(has_shot && !has_pick);
  camera_.setVisible(has_shot && !has_pick);
  pose_.setVisible(has_pick);
  motion_.setVisible(has_pick);
  link_.setVisible(has_pick && link_.IsLinked());
  layer_.setVisible(has_pick && selection.layer().IsValid());
  poses_.setVisible(is_doll);
  drag_.setVisible(has_pick && selection.dot().has_value());
  assert(!(shot_.isVisibleTo(this) && pose_.isVisibleTo(this)));
  assert(has_pick || !link_.isVisibleTo(this));
}

Inspector::~Inspector() {
  assert(widget() == &body_);
  QWidget* body = takeWidget();
  assert(body == &body_);
}

}  // namespace snapper
