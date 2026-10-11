#include "ui/poses_box.h"

#include <QSignalBlocker>

#include <cassert>

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
  assert(buttons_.count() == 3);
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
  assert(managers_.selection != nullptr);
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
