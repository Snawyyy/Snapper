#include "ui/divider_tabs.h"

#include <QCursor>
#include <QPainter>
#include <QPolygonF>

#include <array>
#include <cassert>
#include <memory>

namespace snapper {
namespace {

// The divider colours, in order along the bar.
constexpr std::array<QColor, 6> kColors = {
    QColor(0xe8, 0x45, 0x3c), QColor(0xf0, 0x9a, 0x30),
    QColor(0xcc, 0xc4, 0x3c), QColor(0x3e, 0xb8, 0xa4),
    QColor(0x4a, 0x80, 0xd8), QColor(0x9a, 0x6a, 0xd0)};
// How much darker the tabs that are not open are, and when hovered.
constexpr int kShut = 250;
constexpr int kShutHover = 180;
constexpr double kSlant = 6.0;
constexpr double kSheet = 3.0;
constexpr QColor kInk{0x1a, 0x1a, 0x1c};

QColor ColorOf(int index) {
  assert(index >= 0);
  assert(!kColors.empty());
  return kColors[static_cast<std::size_t>(index) % kColors.size()];
}

}  // namespace

DividerTabBar::DividerTabBar(QWidget* parent) : QTabBar(parent) {
  assert(parent != nullptr);
  assert(kSlant > 0.0);
  setDrawBase(false);
  setExpanding(false);
  setMouseTracking(true);
}

QSize DividerTabBar::tabSizeHint(int index) const {
  const QSize plain = QTabBar::tabSizeHint(index);
  assert(plain.isValid());
  assert(kSheet > 0.0);
  return {plain.width() + static_cast<int>(2.0 * kSlant),
          plain.height() + static_cast<int>(kSheet)};
}

void DividerTabBar::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(count() >= 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const int open = currentIndex();
  // The shut tabs first, so the open one lies over its neighbours.
  for (int index = 0; index < count(); ++index) {
    const bool is_open = index == open;
    if (!is_open) {
      PaintTab(&painter, index);
    }
  }
  const bool has_open = open >= 0;
  if (has_open) {
    painter.fillRect(QRectF(0.0, height() - kSheet, width(), kSheet),
                     ColorOf(open));
    PaintTab(&painter, open);
  }
}

void DividerTabBar::PaintTab(QPainter* painter, int index) const {
  assert(painter != nullptr);
  assert(index >= 0 && index < count());
  const bool is_open = index == currentIndex();
  const QRectF box = QRectF(tabRect(index)).adjusted(0.0, 2.0, 0.0,
                                                      -kSheet);
  const bool is_hot =
      !is_open && box.contains(mapFromGlobal(QCursor::pos()));
  QColor fill = ColorOf(index);
  if (!is_open) {
    fill = fill.darker(is_hot ? kShutHover : kShut);
  }
  // The open tab runs down into the sheet so they read as one piece.
  const double foot = is_open ? box.bottom() + kSheet : box.bottom();
  QPolygonF shape;
  shape << QPointF(box.left(), foot)
        << QPointF(box.left() + kSlant, box.top())
        << QPointF(box.right() - kSlant, box.top())
        << QPointF(box.right(), foot);
  painter->setPen(QPen(fill.darker(140), 1.0));
  painter->setBrush(fill);
  painter->drawPolygon(shape);
  painter->setPen(is_open ? kInk : fill.lighter(260));
  painter->drawText(box, Qt::AlignCenter, tabText(index));
}

DividerTabs::DividerTabs(QWidget* parent) : QTabWidget(parent) {
  assert(parent != nullptr);
  // The widget owns its bar.
  setTabBar(std::make_unique<DividerTabBar>(this).release());
  assert(tabBar() != nullptr);
}

}  // namespace snapper
