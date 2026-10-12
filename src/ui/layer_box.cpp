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
#include "ui/pose_controls.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// The text size fader spans 1 to this many pixels, the outline fader 0
// to kOutlineShown; past them the cap rests at the end.
constexpr double kSizeShown = 300.0;
constexpr double kOutlineShown = 30.0;

}  // namespace

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
  SetUpNumber(&size_, Number::kSize);
  size_.setMinimum(1);
  SetUpNumber(&outline_width_, Number::kSize);
  SetUpNumber(&strength_, Number::kFraction);
  for (int kind = 0; kind < kEffectKindCount; ++kind) {
    effect_.addItem(EffectName(static_cast<EffectKind>(kind)));
  }
  layout_.addRow(tr("Name"), &name_);
  AddNumberPair(&layout_, tr("Frames"), &timing_row_,
                {tr("from"), &start_}, {tr("for"), &length_}, &live_);
  layout_.addRow(QString(), &flip_);
  AddTitle(&layout_, &text_title_, tr("Text"));
  layout_.addRow(tr("Words"), &words_);
  layout_.addRow(MakeFaderRow(tr("Size"), &size_, &live_, kSizeShown,
                              TextLayer().size, this));
  layout_.addRow(QString(), &bold_);
  AddPair(&layout_, tr("Colours"), &colour_row_, &fill_, &outline_);
  layout_.addRow(MakeFaderRow(tr("Outline"), &outline_width_, &live_,
                              kOutlineShown, TextLayer().outline_width,
                              this));
  AddTitle(&layout_, &effect_title_, tr("Effect"));
  AddPair(&layout_, tr("Kind"), &effect_row_, &effect_, &effect_colour_);
  layout_.addRow(MakeFaderRow(tr("Strength"), &strength_, &live_, 1.0, 1.0,
                              this));
  fill_.setToolTip(tr("Fill colour"));
  outline_.setToolTip(tr("Outline colour"));
  strength_.setToolTip(tr("Keyed at the playhead."));
  assert(layout_.rowCount() == 12);
}

void LayerBox::Wire() {
  assert(managers_.IsComplete());
  assert(layout_.rowCount() == 12);
  connect(&name_, &QLineEdit::editingFinished, this, &LayerBox::CommitName);
  for (QSpinBox* box : {&start_, &length_}) {
    MakeLive(box, &live_, tr("Change layer timing"), this,
             [this] { CommitTiming(); });
  }
  connect(&flip_, &QCheckBox::clicked, this, [this](bool is_on) {
    emit Problem(ProblemOf(managers_.stage->FlipAll(
        managers_.selection->shot(), managers_.selection->PickedLayers(),
        is_on)));
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
  // A fader row is the readout's parent: name, fader and readout.
  layout_.setRowVisible(size_.parentWidget(), is_text);
  layout_.setRowVisible(&bold_, is_text);
  layout_.setRowVisible(&colour_row_, is_text);
  layout_.setRowVisible(outline_width_.parentWidget(), is_text);
  layout_.setRowVisible(&effect_title_, is_effect);
  layout_.setRowVisible(&effect_row_, is_effect);
  layout_.setRowVisible(strength_.parentWidget(), is_effect);
}

}  // namespace snapper
