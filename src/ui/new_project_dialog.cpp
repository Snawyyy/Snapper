#include "ui/new_project_dialog.h"

#include <QPainter>
#include <QPixmap>
#include <QPushButton>

#include <array>
#include <memory>
#include <algorithm>
#include <cassert>

#include "ui/form_helpers.h"
#include "ui/theme.h"

namespace snapper {
namespace {

struct Preset final {
  const char* label;
  CanvasSize canvas;
};

// The sizes MVs go out at; the last entry keeps whatever is typed.
constexpr std::array<Preset, 4> kPresets = {{
    {"YouTube\n1920 x 1080", {1920, 1080}},
    {"Shorts, TikTok\n1080 x 1920", {1080, 1920}},
    {"Square\n1080 x 1080", {1080, 1080}},
    {"Custom", {0, 0}},
}};
constexpr int kCustom = static_cast<int>(kPresets.size()) - 1;
constexpr int kFrameLong = 34;

// A frame drawn to the shape of canvas, in colour; a plain box for
// custom.
QPixmap FrameOf(CanvasSize canvas, const QColor& colour) {
  assert(kFrameLong > 4);
  assert(colour.isValid());
  QPixmap pixmap(kFrameLong + 4, kFrameLong + 4);
  pixmap.fill(Qt::transparent);
  const bool is_custom = canvas.width <= 0 || canvas.height <= 0;
  const double wide = is_custom ? 1.0 : canvas.width;
  const double high = is_custom ? 0.7 : canvas.height;
  const double scale = kFrameLong / std::max(wide, high);
  const QSizeF size(wide * scale, high * scale);
  QPainter painter(&pixmap);
  painter.setPen(QPen(colour, 2.0, is_custom ? Qt::DashLine : Qt::SolidLine));
  painter.drawRect(QRectF((pixmap.width() - size.width()) / 2.0,
                          (pixmap.height() - size.height()) / 2.0,
                          size.width(), size.height()));
  return pixmap;
}

}  // namespace

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent),
      layout_(this),
      tiles_layout_(&tiles_),
      buttons_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel) {
  assert(kCustom > 0);
  setWindowTitle(tr("New project"));
  name_.setText(tr("Untitled"));
  tiles_layout_.setContentsMargins(0, 0, 0, 0);
  for (size_t i = 0; i < kPresets.size(); ++i) {
    // A Qt child of the tile row, which owns it.
    QToolButton* tile = std::make_unique<QToolButton>(&tiles_).release();
    QIcon icon;
    icon.addPixmap(FrameOf(kPresets[i].canvas, theme::kText),
                   QIcon::Normal, QIcon::Off);
    icon.addPixmap(FrameOf(kPresets[i].canvas, theme::kPick.lighter(130)),
                   QIcon::Normal, QIcon::On);
    tile->setIcon(icon);
    tile->setIconSize(QSize(kFrameLong + 4, kFrameLong + 4));
    tile->setText(tr(kPresets[i].label));
    tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tile->setCheckable(true);
    tile->setObjectName(QStringLiteral("size_%1").arg(i));
    group_.addButton(tile, static_cast<int>(i));
    tiles_layout_.addWidget(tile);
    tile_[i] = tile;
  }
  for (QSpinBox* side : {&width_, &height_}) {
    side->setRange(kMinCanvasSide, kMaxCanvasSide);
    side->setSingleStep(2);
    // Video sizes are even, so a drag moves two at a time.
    SetScrubUnit(side, 2.0);
    side->setSuffix(tr(" px"));
  }
  layout_.addRow(tr("Name"), &name_);
  layout_.addRow(tr("Size"), &tiles_);
  AddNumberRow(&layout_, tr("Width"), &width_, nullptr);
  AddNumberRow(&layout_, tr("Height"), &height_, nullptr);
  layout_.addRow(&buttons_);
  connect(&buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(&buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(&group_, &QButtonGroup::idClicked, this,
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
  ShowPreset(index);
}

void NewProjectDialog::ShowPreset(int index) {
  assert(index >= -1);
  assert(index < static_cast<int>(kPresets.size()));
  const bool is_tile = index >= 0;
  if (is_tile) {
    tile_[static_cast<size_t>(index)]->setChecked(true);
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
  const int index = group_.checkedId();
  const bool is_off_preset =
      index >= 0 && index != kCustom &&
      !(kPresets[static_cast<size_t>(index)].canvas == now);
  if (is_off_preset) {
    ShowPreset(kCustom);
  }
  assert(group_.checkedId() >= 0 || index < 0);
}

}  // namespace snapper
