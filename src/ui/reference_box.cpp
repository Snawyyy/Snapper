#include "ui/reference_box.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QPalette>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The speed fader spans 0.1x to this; past it the cap rests at the end.
constexpr double kSpeedShown = 2.0;

}  // namespace

ReferenceBox::ReferenceBox(const Managers& managers)
    : QGroupBox(tr("References")), managers_(managers), layout_(this),
      live_(managers.history), find_(tr("Find file...")),
      front_(tr("In front")), shown_(tr("Shown")), flip_(tr("Flip")),
      fit_frame_(tr("Fit to frame")), fit_height_(tr("Fit to height")),
      clear_marks_(tr("Clear")), remove_(tr("Remove reference")) {
  assert(managers_.IsComplete());
  assert(managers_.references != nullptr);
  BuildRows();
  Wire();
  Refresh();
}

void ReferenceBox::BuildRows() {
  assert(layout_.rowCount() == 0);
  assert(managers_.references != nullptr);
  // Like the Pose box: which file on a plate over a sunken frame, then
  // how it looks, how it fits and when it plays, each its own plated
  // section with the settings indented under the title.
  layout_.setVerticalSpacing(theme::kRowGap);
  which_.setObjectName("reference_which");
  which_.setToolTip(tr("The reference these settings change. Click its row "
                       "on the timeline to pick it too."));
  missing_.setWordWrap(true);
  find_.setToolTip(tr("Point it at the file again; its settings stay."));
  auto* file_bar = MakeOwned<QWidget>(this);
  auto* file_column = MakeOwned<QVBoxLayout>(file_bar);
  file_column->setContentsMargins(4, 2, 4, 2);
  file_column->addWidget(&missing_);
  file_column->addWidget(&find_);
  layout_.addRow(MakePlated(tr("Reference"), &which_, this, file_bar));
  BuildLook();
  BuildTime();
  remove_.setToolTip(tr("Take this reference off the shot. Undo brings it "
                        "back."));
  layout_.addRow(&remove_);
  hint_.setWordWrap(true);
  hint_.setText(tr("Seen on this stage only, never in the video. Y shows "
                   "or hides them all; M marks the picked one at the "
                   "playhead; drag its corners on the stage to size it."));
  QPalette quiet = hint_.palette();
  quiet.setColor(QPalette::WindowText, theme::kTextOff);
  hint_.setPalette(quiet);
  layout_.addRow(&hint_);
}

void ReferenceBox::BuildLook() {
  assert(layout_.rowCount() == 1);
  assert(tint_.count() == 0);
  auto* look = MakeOwned<QWidget>(this);
  QFormLayout* rows = SettingsForm(look);
  SetUpNumber(&opacity_, Number::kFraction);
  rows->addRow(MakeFaderRow(tr("Opacity"), &opacity_, &live_, 1.0,
                            kDefaultReferenceOpacity, look));
  front_.setToolTip(tr("Over every layer, to check a silhouette; behind "
                       "them all to trace a pose."));
  shown_.setToolTip(tr("This reference's eye. Y hides every reference at "
                       "once."));
  AddPair(rows, QString(), &switch_row_, &front_, &shown_);
  for (const QString& name : {tr("No tint"), tr("Blue tint"), tr("Red tint")}) {
    tint_.addItem(name);
  }
  assert(tint_.count() == kReferenceTintCount);
  tint_.setToolTip(tr("One color over it, like a light table, so it never "
                      "reads as the art."));
  flip_.setToolTip(tr("Mirror it left to right."));
  AddPair(rows, QString(), &look_row_, &flip_, &tint_);
  fit_frame_.setToolTip(tr("Size it to cover the shot's frame."));
  fit_height_.setToolTip(tr("Size it to the frame's height, keeping its "
                            "shape."));
  AddPair(rows, tr("Fit"), &fit_row_, &fit_frame_, &fit_height_);
  layout_.addRow(PlatedSection(tr("Look"), look, this));
}

void ReferenceBox::BuildTime() {
  assert(layout_.rowCount() == 2);
  assert(managers_.references != nullptr);
  auto* time = MakeOwned<QWidget>(this);
  QFormLayout* rows = SettingsForm(time);
  start_.setRange(0, kMaxFrame);
  start_.setToolTip(tr("The shot frame it starts on. Or drag its bar on "
                       "the timeline."));
  skip_.setRange(0.0, kMaxFrame / static_cast<double>(kFramesPerSecond));
  skip_.setDecimals(2);
  skip_.setSingleStep(1.0 / kFramesPerSecond);
  skip_.setSuffix(tr(" s"));
  rows->addRow(MakeCellRow({MakeSpinCell(tr("Starts"), &start_, &live_, time),
                            MakeSpinCell(tr("Into"), &skip_, &live_, time)},
                           {}, time));
  speed_.setRange(kMinReferenceSpeed, kMaxReferenceSpeed);
  speed_.setDecimals(2);
  speed_.setSingleStep(0.05);
  speed_.setSuffix(tr("x"));
  rows->addRow(MakeFaderRow(tr("Speed"), &speed_, &live_, kSpeedShown, 1.0,
                            time));
  marks_.setWordWrap(true);
  clear_marks_.setToolTip(tr("Remove every marker of this reference."));
  AddPair(rows, tr("Markers"), &marks_row_, &marks_, &clear_marks_);
  layout_.addRow(PlatedSection(tr("Plays"), time, this));
}

void ReferenceBox::Wire() {
  assert(managers_.references != nullptr);
  assert(layout_.rowCount() > 0);
  ReferenceManager* references = managers_.references;
  connect(&which_, &QComboBox::activated, this, [this, references](int at) {
    references->Pick(Shot(), ReferenceId(which_.itemData(at).toInt()));
  });
  connect(&find_, &QPushButton::clicked, this, [this, references] {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Find the reference file"));
    const bool is_picked = !path.isEmpty();
    if (is_picked) {
      Report(ProblemOf(references->Relink(Shot(), Current(), path)));
    }
  });
  MakeLive(&opacity_, &live_, tr("Reference opacity"), this,
           [this, references] {
             Report(ProblemOf(references->SetOpacity(Shot(), Current(),
                                                     opacity_.value())));
           });
  connect(&front_, &QCheckBox::clicked, this, [this, references](bool is_on) {
    Report(ProblemOf(references->SetInFront(Shot(), Current(), is_on)));
  });
  connect(&shown_, &QCheckBox::clicked, this, [this, references](bool is_on) {
    Report(ProblemOf(references->SetShown(Shot(), Current(), is_on)));
  });
  connect(&flip_, &QCheckBox::clicked, this, [this, references](bool is_on) {
    Report(ProblemOf(references->SetFlipped(Shot(), Current(), is_on)));
  });
  connect(&tint_, &QComboBox::activated, this, [this, references](int at) {
    Report(ProblemOf(references->SetTint(
        Shot(), Current(),
        static_cast<ReferenceTint>(
            std::clamp(at, 0, kReferenceTintCount - 1)))));
  });
  connect(&fit_frame_, &QPushButton::clicked, this, [this, references] {
    Report(ProblemOf(
        references->Fit(Shot(), Current(), ReferenceFit::kFrame)));
  });
  connect(&fit_height_, &QPushButton::clicked, this, [this, references] {
    Report(ProblemOf(
        references->Fit(Shot(), Current(), ReferenceFit::kHeight)));
  });
  MakeLive(&start_, &live_, tr("Reference start"), this, [this, references] {
    Report(ProblemOf(
        references->SetStart(Shot(), Current(), Frame(start_.value()))));
  });
  MakeLive(&skip_, &live_, tr("Slide reference"), this, [this, references] {
    Report(ProblemOf(references->SetSkip(Shot(), Current(), skip_.value())));
  });
  MakeLive(&speed_, &live_, tr("Reference speed"), this, [this, references] {
    Report(
        ProblemOf(references->SetSpeed(Shot(), Current(), speed_.value())));
  });
  connect(&clear_marks_, &QPushButton::clicked, this, [this, references] {
    Report(ProblemOf(references->ClearMarks(Shot(), Current())));
  });
  connect(&remove_, &QPushButton::clicked, this, [this, references] {
    Report(ProblemOf(references->Remove(Shot(), Current())));
  });
  connect(references, &ReferenceManager::Changed, this,
          [this] { Refresh(); });
  Follow(managers_, this);
}

ShotId ReferenceBox::Shot() const {
  assert(managers_.selection != nullptr);
  assert(managers_.references != nullptr);
  return managers_.selection->shot();
}

ReferenceId ReferenceBox::Current() const {
  assert(managers_.references != nullptr);
  const ShotId shot = Shot();
  const ReferenceId picked = managers_.references->PickedIn(shot);
  if (picked.IsValid()) {
    return picked;
  }
  const std::vector<Reference> all = managers_.references->Of(shot);
  return all.empty() ? ReferenceId() : all.front().id;
}

void ReferenceBox::Refresh() {
  assert(managers_.references != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = Shot();
  const Reference* reference = managers_.references->Find(shot, Current());
  // Without a reference the inspector hides the box.
  if (reference == nullptr) {
    return;
  }
  {
    const QSignalBlocker quiet(which_);
    which_.clear();
    for (const Reference& one : managers_.references->Of(shot)) {
      which_.addItem(QFileInfo(one.path).fileName(), one.id.value());
    }
    which_.setCurrentIndex(which_.findData(reference->id.value()));
  }
  const bool is_missing = ReferenceManager::IsMissing(*reference);
  missing_.setVisible(is_missing);
  missing_.setText(tr("The file is not at %1. Its settings are kept until "
                      "it is found.")
                       .arg(reference->path));
  find_.setText(is_missing ? tr("Find file...") : tr("Change file..."));
  ShowNumber(&opacity_, reference->opacity);
  for (const auto& [box, is_on] :
       {std::pair{&front_, reference->is_in_front},
        std::pair{&shown_, reference->is_shown},
        std::pair{&flip_, reference->is_flipped}}) {
    const QSignalBlocker quiet(box);
    box->setChecked(is_on);
  }
  {
    const QSignalBlocker quiet(tint_);
    tint_.setCurrentIndex(static_cast<int>(reference->tint));
  }
  ShowNumber(&start_, reference->start.index());
  skip_.setMaximum(std::max(reference->seconds, 0.0));
  ShowNumber(&skip_, reference->skip);
  ShowNumber(&speed_, reference->speed);
  const QString still = reference->IsStill()
                            ? tr("A still picture has no time to move in.")
                            : QString();
  Explain(&skip_, still,
          tr("How far into the file it starts. Or Alt + drag its bar on "
             "the timeline."));
  Explain(&speed_, still, tr("1x plays it at its own speed."));
  marks_.setText(reference->marks.empty()
                     ? tr("None. Press M at a moment.")
                     : tr("%n marked", nullptr,
                          static_cast<int>(reference->marks.size())));
  Explain(&clear_marks_,
          reference->marks.empty() ? tr("It has no markers.") : QString());
}

}  // namespace snapper
