#include "ui/shot_box.h"

#include <QColorDialog>
#include <QSignalBlocker>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {

ShotBox::ShotBox(const Managers& managers)
    : QGroupBox(tr("Shot")), managers_(managers), layout_(this),
      live_(managers.history) {
  assert(managers_.IsComplete());
  name_.setObjectName("shot_name");
  length_.setObjectName("shot_length");
  length_.setRange(1, kMaxFrame);
  length_.setSuffix(tr(" frames"));
  layout_.addRow(tr("Name"), &name_);
  layout_.addRow(tr("Length"), &length_);
  layout_.addRow(tr("Background"), &background_);
  connect(&name_, &QLineEdit::editingFinished, this, [this] {
    const Shot* shot = Focused();
    const bool is_renamed = shot != nullptr && name_.text() != shot->name;
    if (is_renamed) {
      emit Problem(
          ProblemOf(managers_.shots->Rename(shot->id, name_.text())));
    }
  });
  MakeLive(&length_, &live_, tr("Shot length"), this, [this] {
    const Shot* shot = Focused();
    const bool has_shot = shot != nullptr;
    if (has_shot) {
      emit Problem(ProblemOf(managers_.shots->ShiftLength(
          Picked(), length_.value() - shot->length.index())));
    }
  });
  connect(&background_, &QPushButton::clicked, this,
          &ShotBox::PickBackground);
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 3);
}

const Shot* ShotBox::Focused() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  return FindShot(managers_.history->current(), managers_.selection->shot());
}

std::vector<ShotId> ShotBox::Picked() const {
  const auto& shots = managers_.selection->shots();
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  return std::vector<ShotId>(shots.begin(), shots.end());
}

void ShotBox::PickBackground() {
  const Shot* shot = Focused();
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const QColor picked = QColorDialog::getColor(shot->background, this);
  const bool is_picked = picked.isValid();
  if (is_picked) {
    emit Problem(
        ProblemOf(managers_.shots->SetBackgroundAll(Picked(), picked)));
  }
  assert(managers_.shots != nullptr);
}

void ShotBox::Refresh() {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const Shot* shot = Focused();
  const auto count = managers_.selection->shots().size();
  setTitle(count > 1 ? tr("Shot (%1 picked, changes go to each)").arg(count)
                     : tr("Shot"));
  const bool has_shot = shot != nullptr;
  Explain(this, has_shot ? QString() : tr("Add a shot first."));
  if (!has_shot) {
    return;
  }
  const bool is_one = count == 1;
  Explain(&name_, is_one ? QString() : tr("Pick one shot to rename it."));
  {
    const QSignalBlocker quiet_name(name_);
    const bool is_typing = name_.hasFocus();
    if (!is_typing) {
      name_.setText(shot->name);
    }
  }
  ShowNumber(&length_, shot->length.index());
  ShowColour(&background_, shot->background);
}

}  // namespace snapper
