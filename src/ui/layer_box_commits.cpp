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
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return;
  }
  // The fields show the focused layer; the change is added to each.
  const int start_delta = start_.value() - layer->start.index();
  const int length_delta = length_.value() - layer->length.index();
  const bool is_changed = start_delta != 0 || length_delta != 0;
  if (is_changed) {
    emit Problem(ProblemOf(managers_.stage->ShiftTiming(
        managers_.selection->shot(), managers_.selection->PickedLayers(),
        start_delta, length_delta)));
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
  const ShotId shot = managers_.selection->shot();
  const auto picked = managers_.selection->PickedLayers();
  // Words belong to the focused layer alone.
  const bool is_reworded = old->text != words_.toPlainText();
  if (is_reworded) {
    TextLayer text = *old;
    text.text = words_.toPlainText();
    emit Problem(
        ProblemOf(managers_.stage->SetText(shot, layer->id, text)));
  }
  const double size_delta = size_.value() - old->size;
  const double outline_delta = outline_width_.value() - old->outline_width;
  const bool is_resized = size_delta != 0.0 || outline_delta != 0.0;
  if (is_resized) {
    emit Problem(ProblemOf(managers_.stage->ShiftText(
        shot, picked, size_delta, outline_delta)));
  }
  const QColor fill = QColor::fromString(fill_.text());
  const QColor outline = QColor::fromString(outline_.text());
  const bool is_restyled = bold_.isChecked() != old->is_bold ||
                           fill != old->fill || outline != old->outline;
  if (is_restyled) {
    emit Problem(ProblemOf(managers_.stage->StyleText(
        shot, picked, bold_.isChecked(), fill, outline)));
  }
}

void LayerBox::CommitEffect() {
  assert(managers_.stage != nullptr);
  const Layer* layer = Picked();
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return;
  }
  emit Problem(ProblemOf(managers_.stage->SetEffectAll(
      managers_.selection->shot(), managers_.selection->PickedLayers(),
      static_cast<EffectKind>(effect_.currentIndex()),
      QColor::fromString(effect_colour_.text()))));
}

void LayerBox::CommitStrength() {
  assert(managers_.pose != nullptr);
  const Layer* layer = Picked();
  const auto* effect =
      layer != nullptr ? std::get_if<EffectLayer>(&layer->content) : nullptr;
  const auto spot = SpotOf(managers_);
  const bool is_ready = effect != nullptr && spot.has_value() &&
                        spot->shot == managers_.selection->shot();
  if (!is_ready) {
    emit Problem(tr("Move the playhead onto this shot to key strength."));
    return;
  }
  const Shot* shot = FindShot(managers_.history->current(), spot->shot);
  std::vector<TrackRef> tracks;
  for (const LayerId id : managers_.selection->PickedLayers()) {
    const Layer* picked = shot != nullptr ? FindLayer(*shot, id) : nullptr;
    const bool is_effect =
        picked != nullptr &&
        std::holds_alternative<EffectLayer>(picked->content);
    if (is_effect) {
      tracks.push_back({spot->shot, TrackKind::kEffectAmount, id, {}});
    }
  }
  const double delta =
      strength_.value() - Sample(effect->amount, spot->local, 1.0);
  emit Problem(
      ProblemOf(managers_.pose->ShiftAmounts(tracks, spot->local, delta)));
}

}  // namespace snapper
