#include "ui/swatch_grid.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kSwatch = 22;
constexpr int kGap = 3;
// The grid's natural width: eight swatches.
constexpr int kNaturalWidth = 8 * (kSwatch + kGap) + kGap;

}  // namespace

SwatchGrid::SwatchGrid(QWidget* parent) : QWidget(parent) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SwatchGrid::SetColors(std::vector<QColor> colors,
                           const QColor& current) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  colors_ = std::move(colors);
  current_ = current;
  updateGeometry();
  update();
}

int SwatchGrid::Columns() const {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  return std::max(1, (width() - kGap) / (kSwatch + kGap));
}

int SwatchGrid::heightForWidth(int width) const {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  const int columns = std::max(1, (width - kGap) / (kSwatch + kGap));
  const int count = static_cast<int>(colors_.size());
  const int rows = std::max(1, (count + columns - 1) / columns);
  return rows * (kSwatch + kGap) + kGap;
}

QSize SwatchGrid::sizeHint() const {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  return QSize(kNaturalWidth, heightForWidth(kNaturalWidth));
}

QColor SwatchGrid::ColorAt(QPoint point) const {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  const int column = (point.x() - kGap) / (kSwatch + kGap);
  const int row = (point.y() - kGap) / (kSwatch + kGap);
  const bool is_inside = point.x() >= kGap && point.y() >= kGap &&
                         column < Columns();
  const int index = row * Columns() + column;
  const bool is_off = !is_inside || index >= static_cast<int>(colors_.size());
  if (is_off) {
    return {};
  }
  return colors_[static_cast<size_t>(index)];
}

void SwatchGrid::paintEvent(QPaintEvent* event) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
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
    painter.drawRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5));
  }
}

void SwatchGrid::mouseMoveEvent(QMouseEvent* event) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  assert(event != nullptr);
  const QColor color = ColorAt(event->position().toPoint());
  const bool is_new = color != hovered_;
  if (is_new) {
    hovered_ = color;
    update();
    emit Hovered(color);
  }
}

void SwatchGrid::mousePressEvent(QMouseEvent* event) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  assert(event != nullptr);
  // The panel takes the press: a click picks, a drag moves the panel.
  event->ignore();
}

void SwatchGrid::leaveEvent(QEvent* event) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  QWidget::leaveEvent(event);
  hovered_ = QColor();
  update();
  emit Hovered(QColor());
}

void SwatchGrid::contextMenuEvent(QContextMenuEvent* event) {
  assert(kSwatch > 0);
  assert(colors_.size() < 100000);
  assert(event != nullptr);
  emit MenuAsked(ColorAt(event->pos()), event->globalPos());
}

}  // namespace snapper
