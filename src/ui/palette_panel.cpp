#include "ui/palette_panel.h"

#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>

#include <algorithm>
#include <cassert>

#include "edit/palette_manager.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kSwatch = 22;
constexpr int kGap = 3;
constexpr int kPanelWidth = 8 * (kSwatch + kGap) + kGap + 16;
constexpr char kPlaceKey[] = "palette/place";

}  // namespace

SwatchGrid::SwatchGrid() {
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SwatchGrid::SetColors(std::vector<QColor> colors, const QColor& current) {
  colors_ = std::move(colors);
  current_ = current;
  updateGeometry();
  update();
}

int SwatchGrid::Columns() const {
  return std::max(1, (width() - kGap) / (kSwatch + kGap));
}

int SwatchGrid::heightForWidth(int width) const {
  const int columns = std::max(1, (width - kGap) / (kSwatch + kGap));
  const int count = static_cast<int>(colors_.size());
  const int rows = std::max(1, (count + columns - 1) / columns);
  return rows * (kSwatch + kGap) + kGap;
}

QSize SwatchGrid::sizeHint() const {
  return QSize(kPanelWidth - 16, heightForWidth(kPanelWidth - 16));
}

QColor SwatchGrid::ColorAt(QPoint point) const {
  const int column = (point.x() - kGap) / (kSwatch + kGap);
  const int row = (point.y() - kGap) / (kSwatch + kGap);
  const bool is_inside = point.x() >= kGap && point.y() >= kGap &&
                         column < Columns();
  const int index = row * Columns() + column;
  if (!is_inside || index >= static_cast<int>(colors_.size())) {
    return {};
  }
  return colors_[static_cast<size_t>(index)];
}

void SwatchGrid::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const int columns = Columns();
  for (size_t i = 0; i < colors_.size(); ++i) {
    const int index = static_cast<int>(i);
    const QRect box(kGap + (index % columns) * (kSwatch + kGap),
                    kGap + (index / columns) * (kSwatch + kGap), kSwatch,
                    kSwatch);
    const QColor& color = colors_[i];
    const bool is_current = color == current_;
    const bool is_hovered = color == hovered_;
    painter.setPen(is_current   ? QPen(theme::kPick, 2.0)
                   : is_hovered ? QPen(theme::kHandle, 1.5)
                                : QPen(theme::kShadow, 1.0));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 3,
                            3);
  }
}

void SwatchGrid::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QColor color = ColorAt(event->position().toPoint());
  if (color != hovered_) {
    hovered_ = color;
    update();
    emit Hovered(color);
  }
}

void SwatchGrid::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QColor color = ColorAt(event->position().toPoint());
  const bool is_pick = event->button() == Qt::LeftButton && color.isValid();
  if (is_pick) {
    emit Clicked(color, event->modifiers());
  }
}

void SwatchGrid::leaveEvent(QEvent* event) {
  QWidget::leaveEvent(event);
  hovered_ = QColor();
  update();
  emit Hovered(QColor());
}

void SwatchGrid::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  emit MenuAsked(ColorAt(event->pos()), event->globalPos());
}

PalettePanel::PalettePanel(PaletteManager* palette, QString settings_path,
                           QWidget* window)
    : QWidget(window),
      palette_(palette),
      settings_path_(std::move(settings_path)),
      layout_(this) {
  assert(palette_ != nullptr);
  assert(window != nullptr);
  setAttribute(Qt::WA_StyledBackground);
  setFixedWidth(kPanelWidth);
  layout_.setContentsMargins(8, 6, 8, 8);
  layout_.setSpacing(4);
  auto* title_row = new QHBoxLayout(&title_);
  title_row->setContentsMargins(0, 0, 0, 0);
  title_text_.setText(tr("Palette"));
  title_text_.setStyleSheet(QStringLiteral("font-weight: bold;"));
  fold_.setAutoRaise(true);
  fold_.setToolTip(tr("Fold to the current color"));
  close_.setAutoRaise(true);
  close_.setText(QStringLiteral("x"));
  close_.setToolTip(tr("Close the palette"));
  title_row->addWidget(&title_text_, 1);
  title_row->addWidget(&fold_);
  title_row->addWidget(&close_);
  title_.setCursor(Qt::SizeAllCursor);
  for (const QString& name :
       {tr("Piece"), tr("Doll"), tr("Recent"), tr("Added")}) {
    tabs_.addTab(name);
  }
  tabs_.setExpanding(true);
  tabs_.setDrawBase(false);
  empty_.setWordWrap(true);
  empty_.setStyleSheet(QStringLiteral("color: %1;")
                           .arg(theme::kTextOff.name()));
  auto* row = new QHBoxLayout(&current_row_);
  row->setContentsMargins(0, 0, 0, 0);
  current_swatch_.setFixedSize(44, 22);
  current_hex_.setTextInteractionFlags(Qt::TextSelectableByMouse);
  row->addWidget(&current_swatch_);
  row->addWidget(&current_hex_, 1);
  layout_.addWidget(&title_);
  layout_.addWidget(&tabs_);
  layout_.addWidget(&empty_);
  layout_.addWidget(&grid_);
  layout_.addWidget(&current_row_);
  connect(&tabs_, &QTabBar::currentChanged, this, &PalettePanel::Refresh);
  connect(palette_, &PaletteManager::Changed, this, &PalettePanel::Refresh);
  // Draw mode opens and closes the palette through the manager.
  connect(palette_, &PaletteManager::ShownChanged, this,
          &PalettePanel::SetOpen);
  connect(&grid_, &SwatchGrid::Hovered, palette_, &PaletteManager::Hover);
  connect(&grid_, &SwatchGrid::Clicked, palette_, &PaletteManager::Pick);
  connect(&grid_, &SwatchGrid::MenuAsked, this, &PalettePanel::ShowMenu);
  connect(&fold_, &QToolButton::clicked, this,
          [this] { SetFolded(!is_folded_); });
  connect(&close_, &QToolButton::clicked, this, [this] { SetOpen(false); });
  SetFolded(false);
  hide();
}

void PalettePanel::SetOpen(bool is_open) {
  if (is_open == isVisible()) {
    return;
  }
  if (is_open) {
    RestorePlace();
    Refresh();
    show();
    raise();
  } else {
    KeepPlace();
    hide();
  }
  palette_->SetShown(is_open);
  emit OpenChanged(is_open);
}

void PalettePanel::Refresh() {
  assert(palette_ != nullptr);
  std::vector<QColor> colors;
  QString why;
  switch (tab()) {
    case Tab::kPiece:
      colors = palette_->PieceColors();
      why = tr("Pick a piece to see its colors.");
      break;
    case Tab::kDoll:
      colors = palette_->DollColors();
      why = tr("Pick a doll to see its colors.");
      break;
    case Tab::kRecent:
      colors = palette_->RecentColors();
      why = tr("Colors you pick show up here.");
      break;
    case Tab::kAdded:
      colors = palette_->AddedColors();
      why = palette_->DollName().isEmpty()
                ? tr("Pick a doll; added colors are kept with it.")
                : tr("Right click to add a color.");
      break;
  }
  const QColor current = palette_->current();
  grid_.SetColors(colors, current);
  empty_.setText(why);
  empty_.setVisible(!is_folded_ && colors.empty());
  current_swatch_.setStyleSheet(
      QStringLiteral("background: %1; border: 1px solid %2; "
                     "border-radius: 3px;")
          .arg(current.name(), theme::kShadow.name()));
  current_hex_.setText(current.name().toUpper());
  adjustSize();
}

void PalettePanel::SetFolded(bool is_folded) {
  is_folded_ = is_folded;
  fold_.setText(is_folded_ ? QStringLiteral("+") : QStringLiteral("-"));
  tabs_.setVisible(!is_folded_);
  grid_.setVisible(!is_folded_);
  Refresh();
}

void PalettePanel::ShowMenu(const QColor& color, const QPoint& global) {
  QMenu menu;
  menu.setToolTipsVisible(true);
  QAction* add = menu.addAction(tr("Add color..."), this,
                                &PalettePanel::AddColor);
  const QString why = palette_->WhyNoAdd();
  add->setEnabled(why.isEmpty());
  add->setToolTip(why);
  const bool is_added = tab() == Tab::kAdded && color.isValid();
  if (is_added) {
    // The color came from the Added list, so removing it cannot miss.
    menu.addAction(tr("Remove added color"), this, [this, color] {
      static_cast<void>(palette_->RemoveColor(color));
    });
  }
  if (color.isValid()) {
    menu.addAction(tr("Copy hex"), this, [color] {
      QApplication::clipboard()->setText(color.name().toUpper());
    });
  }
  menu.exec(global);
}

void PalettePanel::AddColor() {
  const QColor color =
      QColorDialog::getColor(palette_->current(), this, tr("Add color"));
  if (!color.isValid()) {
    return;
  }
  const auto added = palette_->AddColor(color);
  if (added) {
    tabs_.setCurrentIndex(static_cast<int>(Tab::kAdded));
    palette_->Pick(color);
  }
}

void PalettePanel::KeepPlace() {
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue(kPlaceKey, pos());
}

void PalettePanel::RestorePlace() {
  const QWidget* window = parentWidget();
  assert(window != nullptr);
  const QSettings settings(settings_path_, QSettings::IniFormat);
  const QPoint fallback(window->width() - kPanelWidth - 24, 80);
  QPoint place = settings.value(kPlaceKey, fallback).toPoint();
  // Kept on screen when the window got smaller since.
  place.setX(std::clamp(place.x(), 0, std::max(0, window->width() - 60)));
  place.setY(std::clamp(place.y(), 0, std::max(0, window->height() - 40)));
  move(place);
}

void PalettePanel::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QPoint at = event->position().toPoint();
  const bool is_title = event->button() == Qt::LeftButton &&
                        title_.geometry().contains(at);
  if (is_title) {
    is_moving_ = true;
    grab_ = at;
    raise();
  }
}

void PalettePanel::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (!is_moving_) {
    return;
  }
  const QWidget* window = parentWidget();
  assert(window != nullptr);
  QPoint place = mapToParent(event->position().toPoint() - grab_);
  place.setX(std::clamp(place.x(), 0, std::max(0, window->width() - 60)));
  place.setY(std::clamp(place.y(), 0, std::max(0, window->height() - 40)));
  move(place);
}

void PalettePanel::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (is_moving_) {
    is_moving_ = false;
    KeepPlace();
  }
}

void PalettePanel::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(theme::kFace);
  painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6,
                          6);
}

}  // namespace snapper
