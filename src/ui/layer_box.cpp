#include "ui/layer_box.h"

#include <QSignalBlocker>

#include <cassert>
#include <variant>

#include "anim/sampler.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/live_edit.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

LayerBox::LayerBox(const Managers& managers)
    : QGroupBox(tr("Layer")), managers_(managers), layout_(this),
      live_(managers.history),
      flip_(tr("Mirrored")), bold_(tr("Bold")) {
  assert(managers_.IsComplete());
  BuildRows();
  Wire();
  Refresh();
  assert(layout_.rowCount() > 0);
}

void LayerBox::BuildRows() {
  assert(layout_.rowCount() == 0);
  start_.setRange(0, kMaxFrame);
  start_.setPrefix(tr("from "));
  length_.setRange(0, kMaxFrame);
  length_.setPrefix(tr("for "));
  length_.setSpecialValueText(tr("to the end"));
  words_.setMaximumHeight(60);
  SetUpNumber(&size_, Number::kSize);
  size_.setMinimum(1);
  SetUpNumber(&outline_width_, Number::kSize);
  SetUpNumber(&strength_, Number::kFraction);
  for (int kind = 0; kind < kEffectKindCount; ++kind) {
    effect_.addItem(EffectName(static_cast<EffectKind>(kind)));
  }
  layout_.addRow(tr("Name"), &name_);
  AddPair(&layout_, tr("Frames"), &timing_row_, &start_, &length_);
  layout_.addRow(QString(), &flip_);
  AddTitle(&layout_, &text_title_, tr("Text"));
  layout_.addRow(tr("Words"), &words_);
  AddPair(&layout_, tr("Size"), &size_row_, &size_, &bold_);
  AddPair(&layout_, tr("Colours"), &colour_row_, &fill_, &outline_);
  layout_.addRow(tr("Outline"), &outline_width_);
  AddTitle(&layout_, &effect_title_, tr("Effect"));
  AddPair(&layout_, tr("Kind"), &effect_row_, &effect_, &effect_colour_);
  layout_.addRow(tr("Strength"), &strength_);
  fill_.setToolTip(tr("Fill colour"));
  outline_.setToolTip(tr("Outline colour"));
  strength_.setToolTip(tr("Keyed at the playhead."));
  assert(layout_.rowCount() == 11);
}

void LayerBox::Wire() {
  assert(managers_.IsComplete());
  assert(layout_.rowCount() == 11);
  connect(&name_, &QLineEdit::editingFinished, this, &LayerBox::CommitName);
  for (QSpinBox* box : {&start_, &length_}) {
    MakeLive(box, &live_, tr("Change layer timing"), this,
             [this] { CommitTiming(); });
  }
  connect(&flip_, &QCheckBox::clicked, this, [this](bool is_on) {
    emit Problem(ProblemOf(managers_.stage->SetFlipped(
        managers_.selection->shot(), managers_.selection->layer(), is_on)));
  });
  for (QDoubleSpinBox* box : {&size_, &outline_width_}) {
    MakeLive(box, &live_, tr("Edit text"), this, [this] { CommitText(); });
  }
  connect(&bold_, &QCheckBox::clicked, this, &LayerBox::CommitText);
  // Words land when the field is left, not on every key.
  words_.installEventFilter(this);
  for (QPushButton* button : {&fill_, &outline_, &effect_colour_}) {
    connect(button, &QPushButton::clicked, this, [this, button] {
      const QColor picked =
          AskColour(this, QColor::fromString(button->text()));
      const bool is_picked = picked.isValid();
      if (is_picked) {
        ShowColour(button, picked);
        const bool is_effect = button == &effect_colour_;
        if (is_effect) {
          CommitEffect();
        } else {
          CommitText();
        }
      }
    });
  }
  connect(&effect_, &QComboBox::activated, this, &LayerBox::CommitEffect);
  MakeLive(&strength_, &live_, tr("Effect strength"), this,
           [this] { CommitStrength(); });
  Follow(managers_, this);
}

const Layer* LayerBox::Picked() const {
  const SelectionManager* selection = managers_.selection;
  assert(selection != nullptr);
  const Shot* shot =
      FindShot(managers_.history->current(), selection->shot());
  assert(managers_.history != nullptr);
  return shot != nullptr && selection->layer().IsValid()
             ? FindLayer(*shot, selection->layer())
             : nullptr;
}

void LayerBox::ShowRows(bool is_doll, bool is_text, bool is_effect) {
  assert(!(is_doll && is_text));
  assert(!(is_text && is_effect));
  layout_.setRowVisible(&flip_, is_doll);
  layout_.setRowVisible(&text_title_, is_text);
  layout_.setRowVisible(&words_, is_text);
  layout_.setRowVisible(&size_row_, is_text);
  layout_.setRowVisible(&colour_row_, is_text);
  layout_.setRowVisible(&outline_width_, is_text);
  layout_.setRowVisible(&effect_title_, is_effect);
  layout_.setRowVisible(&effect_row_, is_effect);
  layout_.setRowVisible(&strength_, is_effect);
}

}  // namespace snapper
