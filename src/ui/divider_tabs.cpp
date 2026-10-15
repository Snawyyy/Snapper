#include "ui/divider_tabs.h"

#include <QCursor>
#include <QLinearGradient>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <memory>

#include "ui/theme.h"

namespace snapper {
namespace {

// Each sheet further back from the open one is this much darker; a
// hovered one comes up by kHoverLift.
constexpr int kDarkerPerSheet = 30;
constexpr int kFirstShut = 140;
constexpr int kHoverLift = 25;
// Tabs reach this far over their neighbours, and the front one's
// shadow falls this far onto the ones behind.
constexpr double kOverlap = 8.0;
constexpr double kShadow = 6.0;
constexpr double kSheet = 3.0;
constexpr int kMaxTabs = 64;

}  // namespace

DividerTabBar::DividerTabBar(QWidget* parent) : QTabBar(parent) {
  assert(parent != nullptr);
  assert(kOverlap >= 0.0);
  setDrawBase(false);
  setExpanding(false);
  setMouseTracking(true);
}

QSize DividerTabBar::tabSizeHint(int index) const {
  const QSize plain = QTabBar::tabSizeHint(index);
  assert(plain.isValid());
  assert(kSheet > 0.0);
  return {plain.width(), plain.height() + static_cast<int>(kSheet)};
}

void DividerTabBar::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(count() <= kMaxTabs);
  QPainter painter(this);
  const int open = std::max(0, currentIndex());
  // Stacked like divider sheets: the furthest from the open tab lies at
  // the back, the open one on top of them all.
  for (int depth = kMaxTabs; depth > 0; --depth) {
    for (const int index : {open - depth, open + depth}) {
      const bool is_there = index >= 0 && index < count();
      if (is_there) {
        PaintTab(&painter, index, depth);
      }
    }
  }
  const bool has_open = open < count();
  if (has_open) {
    painter.fillRect(QRectF(0.0, height() - kSheet, width(), kSheet),
                     theme::kFace);
    PaintTab(&painter, open, 0);
  }
}

void DividerTabBar::PaintTab(QPainter* painter, int index, int depth) const {
  assert(painter != nullptr);
  assert(index >= 0 && index < count());
  const bool is_open = depth == 0;
  const QRectF slot = QRectF(tabRect(index)).adjusted(0.0, 2.0, 0.0,
                                                       -kSheet);
  const QRectF box = slot.adjusted(-kOverlap, 0.0, kOverlap, 0.0);
  const bool is_hot =
      !is_open && slot.contains(mapFromGlobal(QCursor::pos()));
  // The open tab runs down into the sheet so they read as one piece.
  const double foot = is_open ? box.bottom() + kSheet : box.bottom();
  const QRectF tab(box.left(), box.top(), box.width(), foot - box.top());
  // Its shadow on the sheets behind, both sides.
  for (const bool is_left : {true, false}) {
    const double edge = is_left ? tab.left() : tab.right();
    const double out = is_left ? edge - kShadow : edge + kShadow;
    QLinearGradient shade(QPointF(edge, 0.0), QPointF(out, 0.0));
    shade.setColorAt(0.0, QColor(0, 0, 0, 120));
    shade.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->fillRect(QRectF(std::min(edge, out), tab.top() + 2.0, kShadow,
                             tab.height() - 2.0),
                      shade);
  }
  QColor fill = theme::kFace;
  if (!is_open) {
    const int dark = kFirstShut + kDarkerPerSheet * (depth - 1);
    fill = fill.darker(is_hot ? dark - kHoverLift : dark);
  }
  painter->fillRect(tab, fill);
  // The sheet's cut edge catches the light.
  painter->setPen(fill.lighter(140));
  painter->drawLine(tab.topLeft(), tab.topRight());
  painter->setPen(is_open ? theme::kText : theme::kTextOff);
  painter->drawText(slot, Qt::AlignCenter, tabText(index));
}

DividerTabs::DividerTabs(QWidget* parent) : QTabWidget(parent) {
  assert(parent != nullptr);
  // The widget owns its bar.
  setTabBar(std::make_unique<DividerTabBar>(this).release());
  assert(tabBar() != nullptr);
}

}  // namespace snapper
