#include "ui/timing_chart_dialog.h"

#include <QSettings>

#include <algorithm>
#include <cassert>
#include <iterator>

#include "edit/document_manager.h"
#include "edit/timing_chart_manager.h"
#include "ui/motion.h"
#include "ui/panel_kit.h"
#include "ui/screen_fit.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// The window's size in ems; FitDialog caps it to the screen.
const QSizeF kDialogEms(32.0, 18.0);

struct PresetName final {
  Spacing spacing;
  const char* label;
};

constexpr PresetName kPresets[] = {
    {Spacing::kEven, "Even"},
    {Spacing::kSlowIn, "Slow in"},
    {Spacing::kSlowOut, "Slow out"},
    {Spacing::kSlowInOut, "Slow in and out"},
    {Spacing::kHalvesA, "Halves (favor A)"},
    {Spacing::kHalvesB, "Halves (favor B)"}};

struct KeyModeName final {
  KeyMode mode;
  const char* label;
};

constexpr KeyModeName kKeyModes[] = {
    {KeyMode::kExisting, "Existing keys only"},
    {KeyMode::kOnes, "Add on 1s"},
    {KeyMode::kTwos, "Add on 2s"},
    {KeyMode::kThrees, "Add on 3s"}};

const char* const kKeyModeSetting = "timingChartKeys";

}  // namespace

TimingChartDialog::TimingChartDialog(const Managers& managers,
                                     QWidget* parent)
    : QDialog(parent),
      managers_(managers),
      strip_(managers.timing),
      apply_(tr("Apply")),
      cancel_(tr("Cancel")) {
  assert(managers_.timing != nullptr);
  assert(managers_.toasts != nullptr);
  setModal(true);
  setWindowTitle(tr("Timing chart"));
  BuildLayout();
  connect(managers_.timing, &TimingChartManager::Opened, this,
          &TimingChartDialog::Show);
  connect(managers_.timing, &TimingChartManager::Closed, this,
          [this] { hide(); });
  connect(&apply_, &QPushButton::clicked, this, &QDialog::accept);
  connect(&cancel_, &QPushButton::clicked, this, &QDialog::reject);
  connect(&presets_, &QComboBox::activated, this,
          &TimingChartDialog::PickPreset);
  connect(&key_modes_, &QComboBox::activated, this,
          &TimingChartDialog::PickKeyMode);
  connect(&strip_, &TimingChartStrip::Edited, this,
          [this] { presets_.setCurrentIndex(-1); });
}

void TimingChartDialog::BuildLayout() {
  assert(layout() == nullptr);
  title_.setWordWrap(true);
  spacing_label_.setText(tr("Spacing"));
  for (const PresetName& preset : kPresets) {
    presets_.addItem(tr(preset.label));
  }
  presets_.setPlaceholderText(tr("Custom"));
  presets_.setToolTip(tr("Ready-made spacings for the ticks."));
  keys_label_.setText(tr("Keys"));
  for (const KeyModeName& entry : kKeyModes) {
    key_modes_.addItem(tr(entry.label));
  }
  key_modes_.setToolTip(
      tr("Which frames between A and B get keys. Existing keys only adds "
         "none and only re-poses the keys already there."));
  apply_.setDefault(true);
  buttons_.addStretch(1);
  buttons_.addWidget(&apply_);
  buttons_.addWidget(&cancel_);
  // Each section's body is owned by its section, the section by the
  // dialog.
  auto* chart = MakeOwned<QWidget>();
  QVBoxLayout* column = StackIn(chart);
  column->setSpacing(theme::kRowGap);
  column->addWidget(&title_);
  column->addWidget(&strip_, 1);
  auto* options = MakeOwned<QWidget>();
  QFormLayout* form = SettingsForm(options);
  form->addRow(&spacing_label_, &presets_);
  form->addRow(&keys_label_, &key_modes_);
  layout_.setSpacing(theme::kRowGap);
  layout_.addWidget(PlatedSection(tr("Chart"), chart, this), 1);
  layout_.addWidget(PlatedSection(tr("Frames between"), options, this));
  layout_.addLayout(&buttons_);
  setLayout(&layout_);
  assert(layout() == &layout_);
}

void TimingChartDialog::Show() {
  assert(managers_.timing->IsOpen());
  assert(!managers_.timing->name().isEmpty());
  title_.setText(tr("%1: frames %2 to %3. Drag the ticks to space the "
                    "frames between.")
                     .arg(managers_.timing->name())
                     .arg(managers_.timing->a().index())
                     .arg(managers_.timing->b().index()));
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  const KeyMode saved = KeyModeFromKey(
      settings.value(kKeyModeSetting).toString().toStdString(),
      KeyMode::kOnes);
  const bool is_new_mode = saved != managers_.timing->key_mode();
  if (is_new_mode) {
    const auto set = managers_.timing->SetKeyMode(saved);
    assert(set.has_value());
    static_cast<void>(set);
  }
  Refresh();
  const bool is_closed = !isVisible();
  if (is_closed) {
    FitDialog(this, kDialogEms);
  }
  show();
  raise();
  activateWindow();
}

void TimingChartDialog::Refresh() {
  assert(managers_.timing->IsOpen());
  assert(static_cast<int>(managers_.timing->spacing()) < kSpacingCount);
  presets_.setCurrentIndex(static_cast<int>(managers_.timing->spacing()));
  key_modes_.setCurrentIndex(static_cast<int>(managers_.timing->key_mode()));
  const QString why_not = managers_.timing->WhyNoApply();
  const bool can_apply = why_not.isEmpty();
  apply_.setEnabled(can_apply);
  apply_.setToolTip(can_apply ? tr("Set the keys from A to B at these "
                                   "spacings.")
                              : why_not);
  strip_.update();
}

void TimingChartDialog::PickKeyMode(int index) {
  assert(index < static_cast<int>(std::size(kKeyModes)));
  assert(managers_.timing != nullptr);
  const bool is_open = managers_.timing->IsOpen();
  const bool is_pick = is_open && index >= 0;
  if (!is_pick) {
    return;
  }
  const bool is_gliding = glide_ != nullptr;
  if (is_gliding) {
    glide_->stop();
  }
  const KeyMode mode = kKeyModes[index].mode;
  const auto set = managers_.timing->SetKeyMode(mode);
  const bool has_failed = !set.has_value();
  if (has_failed) {
    managers_.toasts->Problem(set.error().message);
  }
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  const KeyMode kept = managers_.timing->key_mode();
  settings.setValue(kKeyModeSetting, QString::fromLatin1(KeyModeKey(kept)));
  Refresh();
}

void TimingChartDialog::PickPreset(int index) {
  assert(index < static_cast<int>(std::size(kPresets)));
  assert(managers_.motion != nullptr);
  const bool is_open = managers_.timing->IsOpen();
  const bool is_pick = is_open && index >= 0;
  if (!is_pick) {
    return;
  }
  managers_.timing->ChoosePreset(kPresets[index].spacing);
  const std::vector<double> from = managers_.timing->fractions();
  const std::vector<double> to = SpacingFractions(
      kPresets[index].spacing, static_cast<int>(from.size()));
  const bool is_gliding = glide_ != nullptr;
  if (is_gliding) {
    glide_->stop();
  }
  TimingChartManager* chart = managers_.timing;
  glide_ = managers_.motion->Animate(
      this, MotionKind::kMove, 0.0, 1.0, [chart, from, to](const QVariant& v) {
        const bool is_open_now = chart->IsOpen();
        if (!is_open_now) {
          return;
        }
        std::vector<double> step = to;
        const double t = std::clamp(v.toDouble(), 0.0, 1.0);
        for (size_t i = 0; i < step.size(); ++i) {
          step[i] = from[i] + (to[i] - from[i]) * t;
        }
        const bool is_set = chart->SetFractions(step).has_value();
        assert(is_set);
        static_cast<void>(is_set);
      });
}

void TimingChartDialog::accept() {
  assert(managers_.timing != nullptr);
  assert(managers_.toasts != nullptr);
  const bool is_open = managers_.timing->IsOpen();
  const bool can_apply = is_open && managers_.timing->WhyNoApply().isEmpty();
  if (can_apply) {
    const QString name = managers_.timing->name();
    managers_.timing->Apply();
    managers_.toasts->Done(tr("Timing chart on %1").arg(name));
  }
  QDialog::accept();
}

void TimingChartDialog::reject() {
  assert(managers_.timing != nullptr);
  assert(glide_ == nullptr || glide_->parent() == this);
  const bool is_open = managers_.timing->IsOpen();
  if (is_open) {
    managers_.timing->Cancel();
  }
  QDialog::reject();
}

}  // namespace snapper
