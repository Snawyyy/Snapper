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
      live_(managers.history), background_layout_(&background_row_),
      transparent_(tr("Transparent")) {
  assert(managers_.IsComplete());
  name_.setObjectName("shot_name");
  length_.setObjectName("shot_length");
  length_.setRange(1, kMaxFrame);
  length_.setSuffix(tr(" frames"));
  SetScrubUnit(&length_, 1.0);
  length_seconds_.setObjectName("shot_seconds");
  length_seconds_.setDecimals(2);
  length_seconds_.setRange(SecondsAtFrame(Frame(1)),
                           SecondsAtFrame(Frame(kMaxFrame)));
  length_seconds_.setSingleStep(0.5);
  SetScrubUnit(&length_seconds_, 0.5);
  length_seconds_.setSuffix(tr(" s"));
  length_seconds_.setToolTip(
      tr("Length in seconds, rounded to the nearest frame."));
  layout_.addRow(tr("Name"), &name_);
  AddNumberRow(&layout_, tr("Length"), &length_, &live_);
  AddNumberRow(&layout_, tr("Seconds"), &length_seconds_, &live_);
  transparent_.setObjectName("shot_transparent");
  transparent_.setCheckable(true);
  transparent_.setToolTip(
      tr("See-through: nothing behind the dolls, for putting the shot over "
         "video or into another program. The color is kept for when you "
         "turn this off."));
  background_layout_.setContentsMargins(0, 0, 0, 0);
  background_layout_.addWidget(&background_, 1);
  background_layout_.addWidget(&transparent_);
  layout_.addRow(tr("Background"), &background_row_);
  connect(&transparent_, &QPushButton::clicked, this, [this](bool is_on) {
    emit Problem(
        ProblemOf(managers_.shots->SetTransparentAll(Picked(), is_on)));
  });
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
  MakeLive(&length_seconds_, &live_, tr("Shot length"), this, [this] {
    const Shot* shot = Focused();
    const bool has_shot = shot != nullptr;
    if (has_shot) {
      const Frame typed = FramesNearSeconds(length_seconds_.value());
      emit Problem(ProblemOf(managers_.shots->ShiftLength(
          Picked(), std::max(1, typed.index()) - shot->length.index())));
    }
  });
  connect(&background_, &QPushButton::clicked, this,
          &ShotBox::PickBackground);
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 4);
}

const Shot* ShotBox::Focused() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  return FindShot(managers_.history->current(), managers_.selection->shot());
}

std::vector<ShotId> ShotBox::Picked() const {
  assert(managers_.selection != nullptr);
  const auto& shots = managers_.selection->shots();
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  return std::vector<ShotId>(shots.begin(), shots.end());
}

void ShotBox::PickBackground() {
  assert(managers_.shots != nullptr);
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
  std::vector<double> frames;
  std::vector<double> seconds;
  for (const ShotId id : managers_.selection->shots()) {
    const Shot* picked = FindShot(managers_.history->current(), id);
    const bool is_found = picked != nullptr;
    if (is_found) {
      frames.push_back(picked->length.index());
      seconds.push_back(SecondsAtFrame(picked->length));
    }
  }
  ShowNumber(&length_, shot->length.index(), frames);
  ShowNumber(&length_seconds_, SecondsAtFrame(shot->length), seconds);
  ShowColour(&background_, shot->background);
  const QSignalBlocker quiet_clear(transparent_);
  transparent_.setChecked(shot->is_transparent);
}

}  // namespace snapper
