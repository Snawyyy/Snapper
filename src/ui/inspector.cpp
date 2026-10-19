#include "ui/inspector.h"

#include <QScrollBar>

#include <cassert>
#include <memory>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "edit/storyboard_manager.h"
#include "ui/follow.h"
#include "ui/inspector_show.h"
#include "ui/inspector_title.h"
#include "ui/playhead.h"
#include "ui/scroll_fit.h"
#include "ui/scroll_guard.h"
#include "ui/theme.h"

namespace snapper {

Inspector::Inspector(const Managers& managers)
    : layout_(&body_),
      managers_(managers),
      shot_(managers),
      pose_(managers),
      camera_(managers),
      layer_(managers),
      link_(managers),
      character_(managers) {
  assert(managers.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(theme::kRowGap);
  title_.setObjectName("inspector_title");
  title_.setTextFormat(Qt::RichText);
  layout_.addWidget(&title_);
  hint_.setWordWrap(true);
  hint_.setText(tr("Add a shot in the strip above to start."));
  layout_.addWidget(&hint_);
  layout_.addWidget(&shot_);
  // The camera sits right under its shot, folded closed until opened:
  // most of the time a shot is set up, not filmed. (A new key, so a fold
  // left open by the old default starts closed too.)
  AddFolded(&camera_, "shot_camera", false);
  layout_.addWidget(&pose_);
  AddFolded(&character_, "character");
  const bool has_references = managers_.references != nullptr;
  if (has_references) {
    references_ = std::make_unique<ReferenceBox>(managers_);
    AddFolded(references_.get(), "references");
    connect(references_.get(), &ReferenceBox::Problem, this,
            &Inspector::Problem);
    // Picking a reference row shows its settings.
    connect(managers_.references, &ReferenceManager::Changed, this,
            &Inspector::Refresh);
  }
  const bool has_storyboard =
      managers_.storyboard != nullptr && managers_.draw != nullptr;
  if (has_storyboard) {
    storyboard_ = std::make_unique<StoryboardBox>(managers_);
    AddFolded(storyboard_.get(), "storyboard");
    connect(storyboard_.get(), &StoryboardBox::Problem, this,
            &Inspector::Problem);
    // Picking frames on the timeline shows the box without a stage pick.
    connect(managers_.storyboard, &StoryboardManager::Changed, this,
            &Inspector::Refresh);
  }
  layout_.addWidget(&layer_);
  AddFolded(&link_, "link");
  layout_.addStretch(1);
  // Pressed into the plastic like the model name on a deck: it closes
  // the panel so the space above reads as room, not a missing box.
  badge_.setText(tr("SNAPPER  SN-1  ANIMATION DECK"));
  badge_.setAlignment(Qt::AlignCenter);
  QFont engraved = badge_.font();
  engraved.setBold(true);
  engraved.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
  engraved.setPointSizeF(engraved.pointSizeF() * 0.8);
  badge_.setFont(engraved);
  QPalette pressed = badge_.palette();
  pressed.setColor(QPalette::WindowText, theme::kFaceLight);
  badge_.setPalette(pressed);
  layout_.addWidget(&badge_);
  connect(&pose_, &PoseBox::Problem, this, &Inspector::Problem);
  connect(&character_, &CharacterBox::Problem, this, &Inspector::Problem);
  connect(&shot_, &ShotBox::Problem, this, &Inspector::Problem);
  connect(&camera_, &CameraBox::Problem, this, &Inspector::Problem);
  connect(&layer_, &LayerBox::Problem, this, &Inspector::Problem);
  connect(&link_, &LinkBox::Problem, this, &Inspector::Problem);
  setWidgetResizable(true);
  WatchScrolling(this);
  // Sideways only past the cap KeepWideEnough sets.
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  // The scroll area must not delete body_, a member; it is taken back in
  // the destructor below.
  setWidget(&body_);
  // Wide enough that paired fields (x and y) never get cut off, as boxes
  // come and go, but never more than its share of the screen.
  KeepWideEnough(this);
  Follow(managers_, this);
  Refresh();
  assert(widget() == &body_);
}

void Inspector::AddFolded(QGroupBox* box, const QString& key,
                          bool is_open_at_first) {
  assert(box != nullptr);
  assert(!key.isEmpty());
  auto fold = std::make_unique<FoldSection>(
      box->title(), managers_.motion, managers_.document->SettingsPath(), key,
      &body_, is_open_at_first);
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
  const auto spot = SpotOf(managers_);
  const bool is_here = spot.has_value() && spot->shot == selection.shot();
  const QString words = InspectorTitle(
      project, selection.shot(), selection.picks(),
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
  const bool has_storyboard_box = storyboard_ != nullptr;
  PickExtras extras;
  extras.is_linked = has_pick && link_.IsLinked();
  extras.has_reference =
      managers_.references != nullptr &&
      managers_.references->PickedIn(selection.shot()).IsValid();
  extras.has_storyboard =
      has_storyboard_box &&
      managers_.storyboard->PickedBoard(selection.shot()).IsValid();
  const InspectorShow show =
      ShowFor(project, selection.shot(), selection.picks(), extras);
  hint_.setVisible(!has_shot);
  shot_.setVisible(show.shot);
  Show(&camera_, show.camera);
  pose_.setVisible(show.pose);
  Show(&character_, show.character);
  if (references_ != nullptr) {
    Show(references_.get(), show.references);
  }
  if (storyboard_ != nullptr) {
    Show(storyboard_.get(), show.storyboard);
  }
  Show(&link_, show.link);
  Show(&layer_, show.layer);
  assert(!(shot_.isVisibleTo(this) && pose_.isVisibleTo(this)));
  assert(has_pick || !link_.isVisibleTo(this));
}

Inspector::~Inspector() {
  assert(widget() == &body_);
  QWidget* body = takeWidget();
  assert(body == &body_);
}

}  // namespace snapper
