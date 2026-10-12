#include "ui/reel_text_panel.h"

#include <QSignalBlocker>

#include <array>
#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {
namespace {

constexpr int kMaxTextSize = 1000;
constexpr int kMaxOutline = 100;
constexpr int kMaxMotionFrames = 96;

// In TextSpot order.
constexpr std::array<const char*, kTextSpotCount> kSpotNames = {
    "Bottom", "Top", "Lower third", "Middle"};
// In TextMotion order.
constexpr std::array<const char*, kTextMotionCount> kMotionNames = {
    "Cut", "Fade", "Pop", "Slide up", "Type on"};

}  // namespace

ReelTextPanel::ReelTextPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      fill_(tr("Fill")),
      outline_(tr("Outline")),
      box_(tr("Box")),
      apply_style_(tr("Apply")),
      save_style_(tr("Save as...")),
      remove_style_(tr("Remove")),
      linked_(tr("Keep linked")),
      own_look_(tr("This cue keeps its own look")) {
  assert(managers_.IsComplete());
  BuildRows();
  Wire();
  Refresh();
  assert(layout_.rowCount() > 0);
}

void ReelTextPanel::BuildRows() {
  assert(layout_.rowCount() == 0);
  title_.setWordWrap(true);
  layout_.addRow(&title_);
  layout_.addRow(tr("Font"), &font_);
  size_.setRange(1, kMaxTextSize);
  size_.setSuffix(tr(" px"));
  // Typed numbers land once, not digit by digit.
  for (QSpinBox* box : {&size_, &outline_width_, &appear_length_,
                        &leave_length_}) {
    box->setKeyboardTracking(false);
  }
  bold_.setText(tr("Bold"));
  size_row_.addWidget(&size_, 1);
  size_row_.addWidget(&bold_);
  layout_.addRow(tr("Size"), &size_row_);
  BuildColorsAndPlace();
  BuildMotionAndStyle();
  layout_.addRow(&own_look_);
  own_look_.setToolTip(tr("Off: the cue shares the subtitle track's look, "
                          "so every cue changes together."));
  assert(layout_.rowCount() > 8);
}

void ReelTextPanel::BuildColorsAndPlace() {
  assert(layout_.rowCount() > 0);
  for (QPushButton* button : {&fill_, &outline_, &box_}) {
    button->setCheckable(true);
    button->setToolTip(tr("Click, then pick a color in the floating "
                          "palette."));
    color_row_.addWidget(button);
  }
  no_box_.setText(tr("No box"));
  color_row_.addWidget(&no_box_);
  layout_.addRow(tr("Colors"), &color_row_);
  outline_width_.setRange(0, kMaxOutline);
  outline_width_.setSuffix(tr(" px"));
  layout_.addRow(tr("Outline"), &outline_width_);
  for (int i = 0; i < kTextSpotCount; ++i) {
    QPushButton& spot = spots_[static_cast<size_t>(i)];
    spot.setText(tr(kSpotNames[static_cast<size_t>(i)]));
    spot.setFocusPolicy(Qt::NoFocus);
    spot_row_.addWidget(&spot);
  }
  layout_.addRow(tr("Place"), &spot_row_);
  assert(color_row_.count() == 4);
}

void ReelTextPanel::BuildMotionAndStyle() {
  assert(layout_.rowCount() > 0);
  for (const char* name : kMotionNames) {
    appear_.addItem(tr(name));
    leave_.addItem(tr(name));
  }
  for (QSpinBox* length : {&appear_length_, &leave_length_}) {
    length->setRange(0, kMaxMotionFrames);
    length->setSuffix(tr(" f"));
    length->setToolTip(tr("Frames it takes."));
  }
  appear_row_.addWidget(&appear_, 1);
  appear_row_.addWidget(&appear_length_);
  leave_row_.addWidget(&leave_, 1);
  leave_row_.addWidget(&leave_length_);
  layout_.addRow(tr("In"), &appear_row_);
  layout_.addRow(tr("Out"), &leave_row_);
  style_row_.addWidget(&style_, 1);
  style_row_.addWidget(&apply_style_);
  style_row_.addWidget(&save_style_);
  style_row_.addWidget(&remove_style_);
  layout_.addRow(tr("Style"), &style_row_);
  layout_.addRow(QString(), &linked_);
  linked_.setChecked(true);
  linked_.setToolTip(tr("Linked clips change when the style is saved "
                        "again."));
  assert(appear_.count() == kTextMotionCount);
}

void ReelTextPanel::Wire() {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(&font_, &QFontComboBox::currentFontChanged, this,
          [this](const QFont& font) {
            CommitLook(tr("Text font"), [&font](ReelTextLook* look) {
              look->letters.font_family = font.family();
            });
          });
  connect(&size_, &QSpinBox::valueChanged, this, [this](int size) {
    CommitLook(tr("Text size"), [size](ReelTextLook* look) {
      look->letters.size = size;
    });
  });
  connect(&bold_, &QCheckBox::clicked, this, [this](bool is_bold) {
    CommitLook(tr("Bold text"), [is_bold](ReelTextLook* look) {
      look->letters.is_bold = is_bold;
    });
  });
  connect(&outline_width_, &QSpinBox::valueChanged, this, [this](int width) {
    CommitLook(tr("Text outline"), [width](ReelTextLook* look) {
      look->letters.outline_width = width;
    });
  });
  connect(&fill_, &QPushButton::clicked, this,
          [this] { AimColor(ColorPart::kFill); });
  connect(&outline_, &QPushButton::clicked, this,
          [this] { AimColor(ColorPart::kOutline); });
  connect(&box_, &QPushButton::clicked, this,
          [this] { AimColor(ColorPart::kBox); });
  connect(&no_box_, &QPushButton::clicked, this, [this] {
    CommitLook(tr("No text box"), [](ReelTextLook* look) {
      look->box = QColor(Qt::transparent);
    });
  });
  if (managers_.palette != nullptr) {
    connect(managers_.palette, &PaletteManager::Picked, this,
            [this](const QColor& color) { TakeColor(color); });
  }
  for (int i = 0; i < kTextSpotCount; ++i) {
    connect(&spots_[static_cast<size_t>(i)], &QPushButton::clicked, this,
            [this, i] {
              Report(ProblemOf(managers_.reel->SetTextSpot(
                  PickedText(), static_cast<TextSpot>(i))));
            });
  }
  connect(&appear_, &ChoiceRow::currentIndexChanged, this,
          [this] { CommitMotion(true); });
  connect(&appear_length_, &QSpinBox::valueChanged, this,
          [this] { CommitMotion(true); });
  connect(&leave_, &ChoiceRow::currentIndexChanged, this,
          [this] { CommitMotion(false); });
  connect(&leave_length_, &QSpinBox::valueChanged, this,
          [this] { CommitMotion(false); });
  WireStyles();
}

void ReelTextPanel::WireStyles() {
  assert(managers_.reel != nullptr);
  assert(style_row_.count() == 4);
  connect(&apply_style_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->ApplyTextStyle(
        PickedText(), style_.currentText(), linked_.isChecked())));
  });
  connect(&save_style_, &QPushButton::clicked, this,
          &ReelTextPanel::SaveStyle);
  connect(&remove_style_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->RemoveTextStyle(style_.currentText())));
  });
  connect(&own_look_, &QCheckBox::clicked, this, [this](bool is_own) {
    Report(ProblemOf(managers_.reel->SetOwnLook(PickedText(), is_own)));
  });
}

std::vector<ClipId> ReelTextPanel::PickedText() const {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const Reel& reel = managers_.history->current().reel;
  std::vector<ClipId> picked;
  for (const ClipId id : managers_.selection->clips()) {
    const Clip* clip = ClipOf(reel, id);
    if (clip != nullptr && IsTextClip(*clip)) {
      picked.push_back(id);
    }
  }
  return picked;
}

std::optional<ReelTextLook> ReelTextPanel::ShownLook() const {
  assert(managers_.history != nullptr);
  const Reel& reel = managers_.history->current().reel;
  const std::vector<ClipId> picked = PickedText();
  if (!picked.empty()) {
    return LookOf(reel, *ClipOf(reel, picked.front()));
  }
  const bool has_subtitles = SubtitleTrack(reel) >= 0;
  return has_subtitles ? std::optional<ReelTextLook>(reel.subtitle_look)
                       : std::nullopt;
}

void ReelTextPanel::CommitLook(
    const QString& label, const std::function<void(ReelTextLook*)>& change) {
  assert(!label.isEmpty());
  assert(managers_.reel != nullptr);
  std::optional<ReelTextLook> look = ShownLook();
  if (!look) {
    return;
  }
  change(&*look);
  const std::vector<ClipId> picked = PickedText();
  Report(ProblemOf(picked.empty()
                       ? managers_.reel->SetSubtitleLook(*look, label)
                       : managers_.reel->SetTextLook(picked, *look, label)));
}

void ReelTextPanel::CommitMotion(bool is_appear) {
  assert(managers_.reel != nullptr);
  assert(appear_.currentIndex() >= 0);
  const ChoiceRow& row = is_appear ? appear_ : leave_;
  const QSpinBox& length = is_appear ? appear_length_ : leave_length_;
  Report(ProblemOf(managers_.reel->SetTextMotion(
      PickedText(), is_appear, static_cast<TextMotion>(row.currentIndex()),
      Frame(length.value()))));
}

void ReelTextPanel::AimColor(ColorPart part) {
  assert(part != ColorPart::kNone);
  PaletteManager* palette = managers_.palette;
  const bool has_palette = palette != nullptr;
  if (!has_palette) {
    const std::optional<ReelTextLook> look = ShownLook();
    const QColor picked =
        AskColour(this, look ? look->letters.fill : QColor(Qt::white));
    aimed_ = part;
    if (picked.isValid()) {
      TakeColor(picked);
    }
    aimed_ = ColorPart::kNone;
    Refresh();
    return;
  }
  // A second click on the aimed part stops aiming.
  aimed_ = aimed_ == part ? ColorPart::kNone : part;
  const bool is_aimed = aimed_ != ColorPart::kNone;
  if (is_aimed) {
    palette->SetShown(true);
  }
  Refresh();
}

void ReelTextPanel::TakeColor(const QColor& color) {
  assert(managers_.reel != nullptr);
  const bool is_aimed = aimed_ != ColorPart::kNone && isVisible();
  if (!is_aimed || !color.isValid()) {
    return;
  }
  const ColorPart part = aimed_;
  CommitLook(part == ColorPart::kFill      ? tr("Text color")
             : part == ColorPart::kOutline ? tr("Outline color")
                                           : tr("Text box color"),
             [part, &color](ReelTextLook* look) {
               if (part == ColorPart::kFill) {
                 look->letters.fill = color;
               } else if (part == ColorPart::kOutline) {
                 look->letters.outline = color;
               } else {
                 // A box lets the picture show through a little.
                 look->box = QColor(color.red(), color.green(), color.blue(),
                                    color.alpha() == 255 ? 170 : color.alpha());
               }
             });
}

}  // namespace snapper
