// PalettePanel: the frame, moved by dragging it anywhere, resized by its
// corner, kept where it was left, and fitted to its colors.

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

#include "edit/palette_manager.h"
#include "ui/palette_panel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kSwatch = 22;
constexpr int kGap = 3;
// Eight swatches, the margins and a fold's indent and frame.
constexpr int kPanelWidth = 8 * (kSwatch + kGap) + kGap + 16 + 24;
constexpr char kPlaceKey[] = "palette/place";
constexpr char kSizeKey[] = "palette/size";
constexpr char kRecentKey[] = "palette/recent";
// The resize corner, and the smallest the panel gets.
constexpr int kGrip = 14;
constexpr int kMinWidth = 3 * (kSwatch + kGap) + kGap + 40;
constexpr int kMinHeight = 60;

}  // namespace

void PalettePanel::SetUpFrame() {
  assert(kMinWidth < kPanelWidth);
  assert(kMinHeight > 0);
  setAttribute(Qt::WA_StyledBackground);
  setMinimumWidth(kMinWidth);
  resize(kPanelWidth, height());
  setMouseTracking(true);
  setCursor(Qt::SizeAllCursor);
}

bool PalettePanel::event(QEvent* event) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  assert(event != nullptr);
  const bool is_handled = QWidget::event(event);
  // A fold opening or closing, or colors coming and going, changes how
  // tall the panel must be.
  const bool is_layout = event->type() == QEvent::LayoutRequest;
  if (is_layout) {
    FitHeight();
  }
  return is_handled;
}

void PalettePanel::FitHeight() {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  const int needed = layout_.hasHeightForWidth()
                         ? layout_.totalHeightForWidth(width())
                         : layout_.totalSizeHint().height();
  const int asked = is_folded_ ? 0 : asked_height_;
  const int tall = std::max({needed, asked, kMinHeight});
  const bool is_new = tall != height();
  if (is_new) {
    resize(width(), tall);
  }
}

bool PalettePanel::IsOnGrip(QPoint at) const {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  return at.x() >= width() - kGrip && at.y() >= height() - kGrip;
}

void PalettePanel::KeepPlace() {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue(kPlaceKey, pos());
  settings.setValue(kSizeKey, QSize(width(), asked_height_));
}

void PalettePanel::KeepRecent() const {
  assert(palette_ != nullptr);
  assert(kRecentKey[0] != '\0');
  QStringList names;
  for (const QColor& color : palette_->RecentColors()) {
    names.append(color.name(QColor::HexArgb));
  }
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue(kRecentKey, names);
}

void PalettePanel::LoadRecent() {
  assert(palette_ != nullptr);
  assert(kRecentKey[0] != '\0');
  const QSettings settings(settings_path_, QSettings::IniFormat);
  std::vector<QColor> colors;
  for (const QString& name : settings.value(kRecentKey).toStringList()) {
    colors.emplace_back(QColor::fromString(name));
  }
  palette_->SetRecentColors(std::move(colors));
}

void PalettePanel::RestorePlace() {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  const QWidget* window = parentWidget();
  assert(window != nullptr);
  const QSettings settings(settings_path_, QSettings::IniFormat);
  const QSize kept =
      settings.value(kSizeKey, QSize(kPanelWidth, 0)).toSize();
  const int most_wide = std::max(kMinWidth, window->width() - 20);
  resize(std::clamp(kept.width(), kMinWidth, most_wide), height());
  asked_height_ = std::clamp(kept.height(), 0,
                             std::max(0, window->height() - 20));
  const QPoint fallback(window->width() - width() - 24, 80);
  QPoint place = settings.value(kPlaceKey, fallback).toPoint();
  // Kept on screen when the window got smaller since.
  place.setX(std::clamp(place.x(), 0, std::max(0, window->width() - 60)));
  place.setY(std::clamp(place.y(), 0, std::max(0, window->height() - 40)));
  move(place);
  FitHeight();
}

void PalettePanel::mousePressEvent(QMouseEvent* event) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  assert(event != nullptr);
  const bool is_eyedrop = Eyedrop(*event);
  if (is_eyedrop) {
    return;
  }
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  const QPoint at = event->position().toPoint();
  raise();
  const bool is_grip = IsOnGrip(at);
  if (is_grip) {
    is_resizing_ = true;
    resize_from_ = size();
    resize_at_ = event->globalPosition().toPoint();
    return;
  }
  is_pressed_ = true;
  is_moving_ = false;
  grab_ = at;
  press_keys_ = event->modifiers();
}

void PalettePanel::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_eyedrop = Eyedrop(*event);
  if (is_eyedrop) {
    return;
  }
  const QWidget* window = parentWidget();
  assert(window != nullptr);
  const QPoint at = event->position().toPoint();
  if (is_resizing_) {
    const QPoint by = event->globalPosition().toPoint() - resize_at_;
    const int wide = std::clamp(resize_from_.width() + by.x(), kMinWidth,
                                std::max(kMinWidth, window->width() - x()));
    asked_height_ = std::clamp(resize_from_.height() + by.y(), kMinHeight,
                               std::max(kMinHeight, window->height() - y()));
    resize(wide, height());
    FitHeight();
    return;
  }
  if (!is_pressed_) {
    setCursor(IsOnGrip(at) ? Qt::SizeFDiagCursor : Qt::SizeAllCursor);
    return;
  }
  const bool is_far = (at - grab_).manhattanLength() >=
                      QApplication::startDragDistance();
  is_moving_ = is_moving_ || is_far;
  if (!is_moving_) {
    return;
  }
  QPoint place = mapToParent(at - grab_);
  place.setX(std::clamp(place.x(), 0, std::max(0, window->width() - 60)));
  place.setY(std::clamp(place.y(), 0, std::max(0, window->height() - 40)));
  move(place);
}

void PalettePanel::mouseReleaseEvent(QMouseEvent* event) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  assert(event != nullptr);
  const bool is_eyedrop = Eyedrop(*event);
  if (is_eyedrop) {
    return;
  }
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  if (is_resizing_) {
    is_resizing_ = false;
    KeepPlace();
    return;
  }
  const bool was_click = is_pressed_ && !is_moving_;
  const bool was_move = is_pressed_ && is_moving_;
  is_pressed_ = false;
  is_moving_ = false;
  if (was_move) {
    KeepPlace();
    return;
  }
  const QColor color = was_click ? SwatchAt(grab_) : QColor();
  const bool is_swatch = color.isValid();
  if (is_swatch) {
    palette_->Pick(color, press_keys_);
  }
}

void PalettePanel::paintEvent(QPaintEvent* event) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  assert(event != nullptr);
  QPainter painter(this);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(theme::kFace);
  painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
  // The resize corner: three short lines.
  painter.setPen(QPen(theme::kTextOff, 1.0));
  const QPointF corner(width() - 3.0, height() - 3.0);
  for (int line = 1; line <= 3; ++line) {
    const double reach = 3.0 * line;
    painter.drawLine(corner - QPointF(reach, 0.0),
                     corner - QPointF(0.0, reach));
  }
}

}  // namespace snapper
