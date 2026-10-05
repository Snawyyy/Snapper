#include "ui/motion_box.h"

#include <QSignalBlocker>

#include <cassert>

#include "anim/presets.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/preset_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

MotionBox::MotionBox(const Managers& managers)
    : QGroupBox(tr("Motion")), managers_(managers), layout_(this),
      apply_(tr("Add at playhead")) {
  assert(managers_.IsComplete());
  for (int kind = 0; kind < kMotionPresetCount; ++kind) {
    preset_.addItem(PresetName(static_cast<MotionPreset>(kind)));
  }
  SetUpNumber(&amount_, Number::kSize);
  amount_.setValue(10.0);
  amount_.setToolTip(tr("Pixels for moves, degrees for turns."));
  hold_.addItem(tr("On 1s"), 1);
  hold_.addItem(tr("On 2s"), 2);
  hold_.addItem(tr("On 3s"), 3);
  hold_.setCurrentIndex(1);
  length_.setRange(1, kMaxFrame);
  length_.setValue(24);
  length_.setSuffix(tr(" frames"));
  layout_.addRow(tr("Loop"), &preset_);
  layout_.addRow(tr("Amount"), &amount_);
  layout_.addRow(tr("Timing"), &hold_);
  layout_.addRow(tr("Length"), &length_);
  layout_.addRow(&apply_);
  connect(&apply_, &QPushButton::clicked, this, &MotionBox::Apply);
  Follow(managers_, this);
  Refresh();
}

void MotionBox::Refresh() {
  const auto spot = SpotOf(managers_);
  const bool has_pick =
      spot.has_value() && PickedTrack(managers_, spot->shot).has_value();
  Explain(&apply_, has_pick ? QString()
                            : tr("Pick a piece or layer on the stage first."));
  assert(apply_.isEnabled() == has_pick);
}

void MotionBox::Apply() {
  const auto spot = SpotOf(managers_);
  const auto tracks = managers_.selection->PickedTracks();
  assert(managers_.presets != nullptr);
  const bool is_ready = spot.has_value() && !tracks.empty() &&
                        managers_.selection->shot() == spot->shot;
  if (!is_ready) {
    return;
  }
  const PresetSettings settings{
      static_cast<MotionPreset>(preset_.currentIndex()), amount_.value(),
      hold_.currentData().toInt(), length_.value()};
  emit Problem(ProblemOf(
      managers_.presets->ApplyMotion(tracks, spot->local, settings)));
  assert(settings.hold >= 1);
}

PosesBox::PosesBox(const Managers& managers)
    : QGroupBox(tr("Saved poses")), managers_(managers), layout_(this),
      save_(tr("Save")), use_(tr("Use")), forget_(tr("Forget")) {
  assert(managers_.IsComplete());
  name_.setPlaceholderText(tr("Name for a pose"));
  buttons_.addWidget(&save_);
  buttons_.addWidget(&use_);
  buttons_.addWidget(&forget_);
  layout_.addWidget(&list_);
  layout_.addWidget(&name_);
  layout_.addLayout(&buttons_);
  list_.setMaximumHeight(110);
  connect(&save_, &QPushButton::clicked, this, &PosesBox::Save);
  connect(&use_, &QPushButton::clicked, this, &PosesBox::Use);
  connect(&forget_, &QPushButton::clicked, this, &PosesBox::Forget);
  connect(&list_, &QListWidget::currentRowChanged, this,
          &PosesBox::Refresh);
  connect(&name_, &QLineEdit::textChanged, this, &PosesBox::Refresh);
  connect(managers_.presets, &PresetManager::SavedPosesChanged, this,
          &PosesBox::Refresh);
  Follow(managers_, this);
  Refresh();
}

QString PosesBox::Picked() const {
  const QListWidgetItem* item = list_.currentItem();
  assert(list_.count() >= 0);
  assert(item == nullptr || !item->text().isEmpty());
  return item != nullptr ? item->text() : QString();
}

void PosesBox::Refresh() {
  assert(managers_.presets != nullptr);
  const QStringList saved = managers_.presets->SavedPoses();
  const bool is_listed = list_.count() == saved.size();
  if (!is_listed) {
    const QSignalBlocker quiet(list_);
    list_.clear();
    list_.addItems(saved);
  }
  const auto spot = SpotOf(managers_);
  const bool is_doll =
      spot.has_value() && managers_.selection->shot() == spot->shot &&
      DollOfLayer(managers_.history->current(), spot->shot,
                  managers_.selection->layer()) != nullptr;
  const QString broken = managers_.presets->load_error();
  const QString no_doll = is_doll ? QString()
                                  : tr("Pick a doll on the stage first.");
  Explain(&save_, !broken.isEmpty()            ? broken
                  : name_.text().trimmed().isEmpty() ? tr("Type a name first.")
                                                     : no_doll);
  const QString no_pick = Picked().isEmpty() ? tr("Pick a saved pose first.")
                                             : QString();
  Explain(&use_, no_pick.isEmpty() ? no_doll : no_pick);
  Explain(&forget_, no_pick);
}

void PosesBox::Save() {
  const auto spot = SpotOf(managers_);
  assert(spot.has_value());
  assert(managers_.presets != nullptr);
  emit Problem(ProblemOf(managers_.presets->SavePose(
      name_.text(), spot->shot, managers_.selection->layer(), spot->local)));
  name_.clear();
}

void PosesBox::Use() {
  const auto spot = SpotOf(managers_);
  assert(spot.has_value());
  assert(managers_.presets != nullptr);
  emit Problem(ProblemOf(managers_.presets->ApplyPoseAll(
      Picked(), spot->shot, managers_.selection->PickedLayers(),
      spot->local)));
}

void PosesBox::Forget() {
  assert(managers_.presets != nullptr);
  assert(!Picked().isEmpty());
  emit Problem(ProblemOf(managers_.presets->DeletePose(Picked())));
}

}  // namespace snapper
