// LayerBox's showing and saving.

#include <QEvent>
#include <QSignalBlocker>

#include <cassert>
#include <variant>

#include "anim/sampler.h"
#include "edit/history_manager.h"
#include "edit/pose_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "ui/form_helpers.h"
#include "ui/layer_box.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

bool LayerBox::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const bool is_left = watched == &words_ && event->type() == QEvent::FocusOut;
  if (is_left) {
    CommitText();
  }
  return QGroupBox::eventFilter(watched, event);
}

void LayerBox::Refresh() {
  const Layer* layer = Picked();
  const bool has_layer = layer != nullptr;
  Explain(this, has_layer ? QString() : tr("Pick a layer first."));
  if (!has_layer) {
    ShowRows(false, false, false);
    return;
  }
  const auto* doll = std::get_if<DollLayer>(&layer->content);
  const auto* text = std::get_if<TextLayer>(&layer->content);
  const auto* effect = std::get_if<EffectLayer>(&layer->content);
  ShowRows(doll != nullptr, text != nullptr, effect != nullptr);
  const QSignalBlocker quiet_name(name_);
  const QSignalBlocker quiet_start(start_);
  const QSignalBlocker quiet_length(length_);
  name_.setText(layer->name);
  const bool is_timing_typed = start_.hasFocus() || length_.hasFocus();
  if (!is_timing_typed) {
    start_.setValue(layer->start.index());
    length_.setValue(layer->length.index());
  }
  flip_.setChecked(doll != nullptr && doll->is_flipped);
  const bool is_text = text != nullptr;
  // Never overwrite words while they are being typed.
  const bool can_show_words = is_text && !words_.hasFocus();
  if (can_show_words) {
    const QSignalBlocker quiet(words_);
    words_.setPlainText(text->text);
  }
  if (is_text) {
    ShowNumber(&size_, text->size);
    ShowNumber(&outline_width_, text->outline_width);
    bold_.setChecked(text->is_bold);
    ShowColour(&fill_, text->fill);
    ShowColour(&outline_, text->outline);
  }
  const bool is_effect = effect != nullptr;
  if (is_effect) {
    const QSignalBlocker quiet(effect_);
    effect_.setCurrentIndex(static_cast<int>(effect->kind));
    ShowColour(&effect_colour_, effect->color);
    const auto spot = SpotOf(managers_);
    ShowNumber(&strength_,
               Sample(effect->amount, spot ? spot->local : Frame(0), 1.0));
  }
}

void LayerBox::CommitName() {
  assert(managers_.stage != nullptr);
  const Layer* layer = Picked();
  const bool is_changed = layer != nullptr && layer->name != name_.text();
  if (is_changed) {
    emit Problem(ProblemOf(managers_.stage->Rename(
        managers_.selection->shot(), layer->id, name_.text())));
  }
}

void LayerBox::CommitTiming() {
  assert(managers_.stage != nullptr);
  const Layer* layer = Picked();
  const bool is_changed =
      layer != nullptr && (layer->start.index() != start_.value() ||
                           layer->length.index() != length_.value());
  if (is_changed) {
    emit Problem(ProblemOf(managers_.stage->SetRange(
        managers_.selection->shot(), layer->id, Frame(start_.value()),
        Frame(length_.value()))));
  }
}

void LayerBox::CommitText() {
  assert(managers_.stage != nullptr);
  const Layer* layer = Picked();
  const auto* old = layer != nullptr ? std::get_if<TextLayer>(&layer->content)
                                     : nullptr;
  const bool is_text = old != nullptr;
  if (!is_text) {
    return;
  }
  TextLayer text = *old;
  text.text = words_.toPlainText();
  text.size = size_.value();
  text.is_bold = bold_.isChecked();
  text.fill = QColor::fromString(fill_.text());
  text.outline = QColor::fromString(outline_.text());
  text.outline_width = outline_width_.value();
  const bool is_changed = !(text == *old);
  if (is_changed) {
    emit Problem(ProblemOf(managers_.stage->SetText(
        managers_.selection->shot(), layer->id, text)));
  }
}

void LayerBox::CommitEffect() {
  assert(managers_.stage != nullptr);
  const Layer* layer = Picked();
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return;
  }
  emit Problem(ProblemOf(managers_.stage->SetEffect(
      managers_.selection->shot(), layer->id,
      static_cast<EffectKind>(effect_.currentIndex()),
      QColor::fromString(effect_colour_.text()))));
}

void LayerBox::CommitStrength() {
  assert(managers_.pose != nullptr);
  const Layer* layer = Picked();
  const auto spot = SpotOf(managers_);
  const bool is_ready = layer != nullptr && spot.has_value() &&
                        spot->shot == managers_.selection->shot();
  if (!is_ready) {
    emit Problem(tr("Move the playhead onto this shot to key strength."));
    return;
  }
  const TrackRef track{spot->shot, TrackKind::kEffectAmount, layer->id, {}};
  emit Problem(ProblemOf(
      managers_.pose->SetAmount(track, spot->local, strength_.value())));
}

}  // namespace snapper
