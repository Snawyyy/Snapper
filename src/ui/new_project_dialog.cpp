#include "ui/new_project_dialog.h"

#include <QPushButton>

#include <array>
#include <cassert>

namespace snapper {
namespace {

struct Preset final {
  const char* label;
  CanvasSize canvas;
};

// The sizes MVs go out at; the last entry keeps whatever is typed.
constexpr std::array<Preset, 4> kPresets = {{
    {"1920 x 1080 (YouTube)", {1920, 1080}},
    {"1080 x 1920 (Shorts, TikTok)", {1080, 1920}},
    {"1080 x 1080 (square)", {1080, 1080}},
    {"Custom", {0, 0}},
}};
constexpr int kCustom = static_cast<int>(kPresets.size()) - 1;

}  // namespace

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent),
      layout_(this),
      buttons_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel) {
  assert(kCustom > 0);
  setWindowTitle(tr("New project"));
  name_.setText(tr("Untitled"));
  for (const Preset& preset : kPresets) {
    preset_.addItem(tr(preset.label));
  }
  for (QSpinBox* side : {&width_, &height_}) {
    side->setRange(kMinCanvasSide, kMaxCanvasSide);
    side->setSingleStep(2);
    side->setSuffix(tr(" px"));
  }
  layout_.addRow(tr("Name"), &name_);
  layout_.addRow(tr("Size"), &preset_);
  layout_.addRow(tr("Width"), &width_);
  layout_.addRow(tr("Height"), &height_);
  layout_.addRow(&buttons_);
  connect(&buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(&buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(&preset_, &QComboBox::currentIndexChanged, this,
          &NewProjectDialog::PickPreset);
  connect(&name_, &QLineEdit::textChanged, this, &NewProjectDialog::Refresh);
  for (QSpinBox* side : {&width_, &height_}) {
    connect(side, &QSpinBox::valueChanged, this, &NewProjectDialog::Refresh);
  }
  PickPreset(0);
  assert(WhyNotReady().isEmpty());
}

CanvasSize NewProjectDialog::canvas() const {
  const CanvasSize size{width_.value(), height_.value()};
  assert(size.width >= kMinCanvasSide);
  assert(size.height >= kMinCanvasSide);
  return size;
}

QString NewProjectDialog::WhyNotReady() const {
  const bool has_name = !name().isEmpty();
  const bool is_even = IsValidCanvas(canvas());
  assert(width_.minimum() == kMinCanvasSide);
  assert(height_.maximum() == kMaxCanvasSide);
  if (!has_name) {
    return tr("Give the project a name.");
  }
  return is_even ? QString() : tr("Video sizes must be even numbers.");
}

void NewProjectDialog::SetCanvas(CanvasSize canvas) {
  assert(canvas.width >= 0);
  assert(canvas.height >= 0);
  {
    const QSignalBlocker quiet_width(width_);
    const QSignalBlocker quiet_height(height_);
    width_.setValue(canvas.width);
    height_.setValue(canvas.height);
  }
  Refresh();
}

void NewProjectDialog::PickPreset(int index) {
  assert(index >= -1);
  assert(index < static_cast<int>(kPresets.size()));
  const bool is_preset = index >= 0 && index != kCustom;
  if (is_preset) {
    SetCanvas(kPresets[static_cast<size_t>(index)].canvas);
  }
}

void NewProjectDialog::Refresh() {
  QPushButton* ok = buttons_.button(QDialogButtonBox::Ok);
  assert(ok != nullptr);
  const QString why_not = WhyNotReady();
  ok->setEnabled(why_not.isEmpty());
  ok->setToolTip(why_not);
  // Typing a size by hand switches the preset to Custom.
  const CanvasSize now = canvas();
  const int index = preset_.currentIndex();
  const bool is_off_preset =
      index >= 0 && index != kCustom &&
      !(kPresets[static_cast<size_t>(index)].canvas == now);
  if (is_off_preset) {
    const QSignalBlocker quiet(preset_);
    preset_.setCurrentIndex(kCustom);
  }
  assert(preset_.currentIndex() >= 0);
}

}  // namespace snapper
