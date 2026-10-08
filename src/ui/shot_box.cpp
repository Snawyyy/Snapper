#include "ui/shot_box.h"

#include <QColorDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// A new swipe or fade starts this long.
constexpr int kDefaultTransition = 4;

}  // namespace

ShotBox::ShotBox(const Managers& managers)
    : QGroupBox(tr("Shot")), managers_(managers), layout_(this),
      live_(managers.history) {
  assert(managers_.IsComplete());
  name_.setObjectName("shot_name");
  length_.setObjectName("shot_length");
  transition_.setObjectName("transition");
  transition_length_.setObjectName("transition_length");
  length_.setRange(1, kMaxFrame);
  length_.setSuffix(tr(" frames"));
  length_seconds_.setObjectName("shot_seconds");
  length_seconds_.setDecimals(2);
  length_seconds_.setRange(SecondsAtFrame(Frame(1)),
                           SecondsAtFrame(Frame(kMaxFrame)));
  length_seconds_.setSingleStep(0.5);
  length_seconds_.setSuffix(tr(" s"));
  length_seconds_.setToolTip(
      tr("Length in seconds, rounded to the nearest frame."));
  transition_length_.setRange(1, kFramesPerSecond * 4);
  transition_length_.setSuffix(tr(" frames"));
  for (int kind = 0; kind < kTransitionKindCount; ++kind) {
    transition_.addItem(TransitionName(static_cast<TransitionKind>(kind)));
  }
  layout_.addRow(tr("Name"), &name_);
  AddPair(&layout_, tr("Length"), &length_row_, &length_,
          &length_seconds_);
  layout_.addRow(tr("Background"), &background_);
  layout_.addRow(tr("Into next"), &transition_);
  layout_.addRow(tr("Overlap"), &transition_length_);
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
  connect(&transition_, &QComboBox::activated, this, [this](int index) {
    const auto kind = static_cast<TransitionKind>(index);
    const bool is_cut = kind == TransitionKind::kCut;
    const Shot* shot = Focused();
    const int now = shot != nullptr ? shot->transition.length.index() : 0;
    SetTransition(kind, is_cut ? 0 : std::max(now, kDefaultTransition));
  });
  MakeLive(&transition_length_, &live_, tr("Change transition"), this,
           [this] {
             const Shot* shot = Focused();
             const bool has_shot = shot != nullptr;
             if (has_shot) {
               SetTransition(shot->transition.kind,
                             transition_length_.value());
             }
           });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 5);
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

void ShotBox::SetTransition(TransitionKind kind, int frames) {
  assert(frames >= 0);
  assert(static_cast<int>(kind) < kTransitionKindCount);
  emit Problem(ProblemOf(
      managers_.shots->SetTransitionAll(Picked(), {kind, Frame(frames)})));
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
  const Shot* shot = Focused();
  const auto count = managers_.selection->shots().size();
  setTitle(count > 1 ? tr("Shot (%1 picked, changes go to each)").arg(count)
                     : tr("Shot"));
  const bool has_shot = shot != nullptr;
  Explain(this, has_shot ? QString() : tr("Add a shot first."));
  if (!has_shot) {
    return;
  }
  const Project& project = managers_.history->current();
  const bool is_last = project.shots.back()->id == shot->id;
  const bool is_one = count == 1;
  Explain(&name_, is_one ? QString() : tr("Pick one shot to rename it."));
  {
    const QSignalBlocker quiet_name(name_);
    const QSignalBlocker quiet_kind(transition_);
    const bool is_typing = name_.hasFocus();
    if (!is_typing) {
      name_.setText(shot->name);
    }
    transition_.setCurrentIndex(static_cast<int>(shot->transition.kind));
  }
  ShowNumber(&length_, shot->length.index());
  ShowNumber(&length_seconds_, SecondsAtFrame(shot->length));
  ShowNumber(&transition_length_,
             std::max(1, shot->transition.length.index()));
  ShowColour(&background_, shot->background);
  Explain(&transition_,
          is_last ? tr("The last shot has nothing after it.") : QString());
  const bool is_cut = shot->transition.kind == TransitionKind::kCut;
  Explain(&transition_length_,
          is_last  ? tr("The last shot has nothing after it.")
          : is_cut ? tr("A cut has no overlap; pick a swipe or fade.")
                   : QString());
}

}  // namespace snapper
