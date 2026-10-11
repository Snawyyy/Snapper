#include "ui/inspector.h"

#include <QScrollBar>

#include <cassert>
#include <memory>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/inspector_title.h"
#include "ui/playhead.h"

namespace snapper {

Inspector::Inspector(const Managers& managers)
    : layout_(&body_),
      managers_(managers),
      shot_(managers),
      pose_(managers),
      groups_(managers),
      drag_(managers),
      point_motion_(managers),
      camera_(managers),
      layer_(managers),
      link_(managers),
      poses_(managers) {
  assert(managers.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(4);
  title_.setObjectName("inspector_title");
  title_.setTextFormat(Qt::RichText);
  layout_.addWidget(&title_);
  hint_.setWordWrap(true);
  hint_.setText(tr("Add a shot in the strip above to start."));
  layout_.addWidget(&hint_);
  layout_.addWidget(&shot_);
  layout_.addWidget(&pose_);
  AddFolded(&groups_, "groups");
  layout_.addWidget(&drag_);
  AddFolded(&point_motion_, "dot_animation");
  layout_.addWidget(&layer_);
  AddFolded(&camera_, "camera");
  AddFolded(&link_, "link");
  AddFolded(&poses_, "saved_poses");
  layout_.addStretch(1);
  connect(&pose_, &PoseBox::Problem, this, &Inspector::Problem);
  connect(&shot_, &ShotBox::Problem, this, &Inspector::Problem);
  connect(&groups_, &GroupBox::Problem, this, &Inspector::Problem);
  connect(&drag_, &DragBox::Problem, this, &Inspector::Problem);
  connect(&point_motion_, &PointMotionBox::Problem, this,
          &Inspector::Problem);
  connect(&camera_, &CameraBox::Problem, this, &Inspector::Problem);
  connect(&layer_, &LayerBox::Problem, this, &Inspector::Problem);
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

void Inspector::AddFolded(QGroupBox* box, const QString& key) {
  assert(box != nullptr);
  assert(!key.isEmpty());
  auto fold = std::make_unique<FoldSection>(
      box->title(), managers_.motion, managers_.document->SettingsPath(), key,
      &body_, true);
  box->setTitle(QString());
  box->setFlat(true);
  fold->form()->addRow(box);
  FoldSection* heading = fold.release();
  layout_.addWidget(heading);
  folds_.emplace_back(box, heading);
  assert(!folds_.empty());
}

void Inspector::Show(QGroupBox* box, bool is_shown) {
  assert(box != nullptr);
  assert(folds_.size() < 100);
  // Showing a widget that is shown already still costs a layout pass,
  // and this runs on every change.
  const bool is_box_new = box->isHidden() == is_shown;
  if (is_box_new) {
    box->setVisible(is_shown);
  }
  for (const auto& [folded, heading] : folds_) {
    const bool is_it = folded == box && heading->isHidden() == is_shown;
    if (is_it) {
      heading->setVisible(is_shown);
    }
  }
}

void Inspector::Refresh() {
  const SelectionManager& selection = *managers_.selection;
  const Project& project = managers_.history->current();
  const bool has_shot = FindShot(project, selection.shot()) != nullptr;
  const bool has_pick = has_shot && !selection.picks().empty();
  const bool is_doll =
      has_pick && DollOfLayer(project, selection.shot(), selection.layer()) !=
                      nullptr;
  const auto spot = SpotOf(managers_);
  const bool is_here = spot.has_value() && spot->shot == selection.shot();
  const QString words = InspectorTitle(
      project, selection.shot(), selection.picks(),
      static_cast<int>(selection.dots().size()),
      is_here ? std::optional<Frame>(spot->local) : std::nullopt);
  title_.setVisible(!words.isEmpty());
  // Setting rich text lays it out again: only when it says something new.
  const QString html = QStringLiteral("<span style=\"color:#d8304f\">&#9679;"
                                      "</span> <b>%1</b>")
                           .arg(words.toHtmlEscaped());
  const bool is_new_title = html != title_.text();
  if (is_new_title) {
    title_.setText(html);
  }
  hint_.setVisible(!has_shot);
  shot_.setVisible(has_shot && !has_pick);
  Show(&camera_, has_shot && !has_pick);
  pose_.setVisible(has_pick);
  Show(&link_, has_pick && link_.IsLinked());
  Show(&layer_, has_pick && selection.layer().IsValid());
  Show(&poses_, is_doll);
  Show(&groups_, is_doll);
  const bool has_dot = has_pick && selection.dot().has_value();
  drag_.setVisible(has_dot);
  Show(&point_motion_, has_dot);
  assert(!(shot_.isVisibleTo(this) && pose_.isVisibleTo(this)));
  assert(has_pick || !link_.isVisibleTo(this));
}

Inspector::~Inspector() {
  assert(widget() == &body_);
  QWidget* body = takeWidget();
  assert(body == &body_);
}

}  // namespace snapper
