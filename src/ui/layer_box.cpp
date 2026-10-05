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
  length_.setRange(0, kMaxFrame);
  length_.setSpecialValueText(tr("to the end"));
  words_.setMaximumHeight(60);
  SetUpNumber(&size_, 1, 2000, 0, tr(" px"));
  SetUpNumber(&outline_width_, 0, 200, 1, tr(" px"));
  SetUpNumber(&strength_, 0, 1, 2, QString());
  strength_.setSingleStep(0.05);
  for (int kind = 0; kind < kEffectKindCount; ++kind) {
    effect_.addItem(EffectName(static_cast<EffectKind>(kind)));
  }
  layout_.addRow(tr("Name"), &name_);
  layout_.addRow(tr("From frame"), &start_);
  layout_.addRow(tr("For frames"), &length_);
  layout_.addRow(QString(), &flip_);
  layout_.addRow(tr("Words"), &words_);
  layout_.addRow(tr("Size"), &size_);
  layout_.addRow(QString(), &bold_);
  layout_.addRow(tr("Fill"), &fill_);
  layout_.addRow(tr("Outline"), &outline_);
  layout_.addRow(tr("Outline width"), &outline_width_);
  layout_.addRow(tr("Effect"), &effect_);
  layout_.addRow(tr("Colour"), &effect_colour_);
  layout_.addRow(tr("Strength at playhead"), &strength_);
  assert(layout_.rowCount() == 13);
}

void LayerBox::Wire() {
  assert(managers_.IsComplete());
  assert(layout_.rowCount() == 13);
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
  for (QWidget* row : {static_cast<QWidget*>(&words_),
                       static_cast<QWidget*>(&size_),
                       static_cast<QWidget*>(&bold_),
                       static_cast<QWidget*>(&fill_),
                       static_cast<QWidget*>(&outline_),
                       static_cast<QWidget*>(&outline_width_)}) {
    layout_.setRowVisible(row, is_text);
  }
  for (QWidget* row : {static_cast<QWidget*>(&effect_),
                       static_cast<QWidget*>(&effect_colour_),
                       static_cast<QWidget*>(&strength_)}) {
    layout_.setRowVisible(row, is_effect);
  }
}

}  // namespace snapper
