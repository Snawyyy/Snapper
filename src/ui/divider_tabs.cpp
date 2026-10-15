#include "ui/divider_tabs.h"

#include <QCursor>
#include <QPainter>

#include <cassert>
#include <memory>

#include "ui/theme.h"

namespace snapper {
namespace {

// How much darker the tabs that are not open are, and when hovered.
constexpr int kShut = 175;
constexpr int kShutHover = 135;
// Room between neighbouring tabs, as between divider sheets.
constexpr double kGap = 2.0;
constexpr double kSheet = 3.0;

}  // namespace

DividerTabBar::DividerTabBar(QWidget* parent) : QTabBar(parent) {
  assert(parent != nullptr);
  assert(kGap >= 0.0);
  setDrawBase(false);
  setExpanding(false);
  setMouseTracking(true);
}

QSize DividerTabBar::tabSizeHint(int index) const {
  const QSize plain = QTabBar::tabSizeHint(index);
  assert(plain.isValid());
  assert(kSheet > 0.0);
  return {plain.width() + static_cast<int>(kGap),
          plain.height() + static_cast<int>(kSheet)};
}

void DividerTabBar::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(count() >= 0);
  QPainter painter(this);
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
                     theme::kFace);
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
  QColor fill = theme::kFace;
  if (!is_open) {
    fill = fill.darker(is_hot ? kShutHover : kShut);
  }
  // The open tab runs down into the sheet so they read as one piece.
  const double foot = is_open ? box.bottom() + kSheet : box.bottom();
  const QRectF tab(box.left(), box.top(), box.width() - kGap,
                   foot - box.top());
  painter->setPen(Qt::NoPen);
  painter->fillRect(tab, fill);
  painter->setPen(is_open ? theme::kText : theme::kTextOff);
  painter->drawText(tab.adjusted(0.0, 0.0, 0.0, is_open ? -kSheet : 0.0),
                    Qt::AlignCenter, tabText(index));
}

DividerTabs::DividerTabs(QWidget* parent) : QTabWidget(parent) {
  assert(parent != nullptr);
  // The widget owns its bar.
  setTabBar(std::make_unique<DividerTabBar>(this).release());
  assert(tabBar() != nullptr);
}

}  // namespace snapper
