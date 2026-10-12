// ReelTextPanel's showing: the fields follow the pick and the project.

#include <QInputDialog>
#include <QPixmap>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <variant>

#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/reel_text_panel.h"

namespace snapper {
namespace {

constexpr int kSwatch = 12;

QPixmap Swatch(const QColor& color) {
  assert(color.isValid());
  assert(kSwatch > 0);
  QPixmap swatch(kSwatch, kSwatch);
  swatch.fill(color);
  return swatch;
}

}  // namespace

void ReelTextPanel::Refresh() {
  assert(managers_.history != nullptr);
  assert(managers_.reel != nullptr);
  const std::vector<ClipId> picked = PickedText();
  const std::optional<ReelTextLook> look = ShownLook();
  title_.setText(picked.size() > 1 ? tr("%1 text clips").arg(picked.size())
                 : !picked.empty() ? tr("Text clip")
                 : look ? tr("Subtitle look, shared by every cue")
                        : tr("Pick a text clip, or add one with T+ "
                             "(Ctrl+T)."));
  const QString no_look =
      look ? QString() : tr("Pick a text clip first (Ctrl+T adds one).");
  for (QWidget* field : std::initializer_list<QWidget*>{
           &font_, &size_, &bold_, &fill_, &outline_, &box_, &no_box_,
           &outline_width_}) {
    Explain(field, no_look);
  }
  const QString no_clip =
      picked.empty() ? tr("Pick a text clip first.") : QString();
  for (QWidget* field : std::initializer_list<QWidget*>{
           &appear_, &appear_length_, &leave_, &leave_length_, &save_style_,
           &linked_}) {
    Explain(field, no_clip);
  }
  for (QPushButton& spot : spots_) {
    Explain(&spot, no_clip);
  }
  if (look) {
    ShowLook(*look);
  }
  ShowMotion(picked);
  fill_.setChecked(aimed_ == ColorPart::kFill);
  outline_.setChecked(aimed_ == ColorPart::kOutline);
  box_.setChecked(aimed_ == ColorPart::kBox);
}

void ReelTextPanel::ShowLook(const ReelTextLook& look) {
  assert(look.letters.size > 0.0);
  assert(look.box.isValid());
  {
    const QSignalBlocker quiet_font(&font_);
    const QSignalBlocker quiet_size(&size_);
    const QSignalBlocker quiet_width(&outline_width_);
    font_.setCurrentFont(QFont(look.letters.font_family));
    size_.setValue(static_cast<int>(look.letters.size));
    outline_width_.setValue(static_cast<int>(look.letters.outline_width));
  }
  bold_.setChecked(look.letters.is_bold);
  fill_.setIcon(Swatch(look.letters.fill));
  outline_.setIcon(Swatch(look.letters.outline));
  box_.setIcon(Swatch(look.box.alpha() > 0 ? look.box : QColor(Qt::gray)));
}

void ReelTextPanel::ShowMotion(const std::vector<ClipId>& picked) {
  assert(managers_.history != nullptr);
  assert(picked.size() < 1000000);
  const Reel& reel = managers_.history->current().reel;
  const Clip* first = picked.empty() ? nullptr : ClipOf(reel, picked.front());
  const QString current_style = style_.currentText();
  {
    const QSignalBlocker quiet(&style_);
    style_.clear();
    for (const TextStyle& style : reel.text_styles) {
      style_.addItem(style.name);
    }
    const auto* text =
        first != nullptr ? std::get_if<TextSource>(&first->source) : nullptr;
    const QString shown = text != nullptr && !text->style.isEmpty()
                              ? text->style
                              : current_style;
    style_.setCurrentIndex(std::max(style_.findText(shown), 0));
  }
  const QString no_style =
      style_.count() == 0 ? tr("Save a style first.") : QString();
  Explain(&apply_style_, picked.empty() ? tr("Pick a text clip first.")
                                        : no_style);
  Explain(&remove_style_, no_style);
  const int subtitles = SubtitleTrack(reel);
  const bool is_cue = first != nullptr && subtitles >= 0 &&
                      FindClip(reel, first->id).track == subtitles;
  layout_.setRowVisible(&own_look_, is_cue);
  if (first == nullptr) {
    return;
  }
  const auto& text = std::get<TextSource>(first->source);
  own_look_.setChecked(text.is_own_look);
  const QSignalBlocker quiet_appear(&appear_);
  const QSignalBlocker quiet_leave(&leave_);
  const QSignalBlocker quiet_in(&appear_length_);
  const QSignalBlocker quiet_out(&leave_length_);
  appear_.setCurrentIndex(static_cast<int>(text.appear));
  leave_.setCurrentIndex(static_cast<int>(text.leave));
  appear_length_.setValue(text.appear_length.index());
  leave_length_.setValue(text.leave_length.index());
}

void ReelTextPanel::SaveStyle() {
  assert(managers_.reel != nullptr);
  assert(style_row_.count() == 4);
  const std::vector<ClipId> picked = PickedText();
  if (picked.empty()) {
    Report(tr("Pick a text clip first."));
    return;
  }
  bool is_ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Save style"),
      tr("Name the look (saving under a used name changes every clip "
         "linked to it):"),
      QLineEdit::Normal,
      style_.currentText().isEmpty() ? tr("Title") : style_.currentText(),
      &is_ok);
  const bool is_named = is_ok && !name.trimmed().isEmpty();
  if (is_named) {
    Report(ProblemOf(managers_.reel->SaveTextStyle(name, picked.front())));
  }
}

}  // namespace snapper
