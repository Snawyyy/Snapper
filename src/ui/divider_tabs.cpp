#include "ui/divider_tabs.h"

#include <QCursor>
#include <QLinearGradient>
#include <QPaintEvent>
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
// The sheets under the open one: how many peek out and how far each.
constexpr int kUnderSheets = 2;
constexpr double kUnderStep = 3.0;
constexpr int kPagePad = 6;

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
  // Its shadow on the sheets behind, both sides, and its cut edges end
  // where it meets its own sheet: no shadow on the same paper.
  const QRectF edges(box.left(), box.top(), box.width(),
                     box.bottom() - box.top());
  for (const bool is_left : {true, false}) {
    const double edge = is_left ? edges.left() : edges.right();
    const double out = is_left ? edge - kShadow : edge + kShadow;
    QLinearGradient shade(QPointF(edge, 0.0), QPointF(out, 0.0));
    shade.setColorAt(0.0, QColor(0, 0, 0, 120));
    shade.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->fillRect(QRectF(std::min(edge, out), edges.top() + 2.0,
                             kShadow, edges.height() - 2.0),
                      shade);
  }
  QColor fill = theme::kFace;
  if (!is_open) {
    const int dark = kFirstShut + kDarkerPerSheet * (depth - 1);
    fill = fill.darker(is_hot ? dark - kHoverLift : dark);
  }
  painter->fillRect(tab, fill);
  // The paper's cut edges: lit at the top and left, dark at the right.
  painter->setPen(fill.lighter(140));
  painter->drawLine(tab.topLeft(), tab.topRight());
  painter->drawLine(edges.topLeft(), edges.bottomLeft());
  painter->setPen(fill.darker(150));
  painter->drawLine(edges.topRight(), edges.bottomRight());
  painter->setPen(is_open ? theme::kText : theme::kTextOff);
  painter->drawText(slot, Qt::AlignCenter, tabText(index));
}

DividerTabs::DividerTabs(QWidget* parent) : QTabWidget(parent) {
  assert(parent != nullptr);
  // The widget owns its bar.
  setTabBar(std::make_unique<DividerTabBar>(this).release());
  assert(tabBar() != nullptr);
}

QMargins DividerTabs::PageMargins() {
  assert(kPagePad > 0);
  const int under = static_cast<int>(kUnderSheets * kUnderStep);
  assert(under >= 0);
  return {kPagePad, kPagePad, kPagePad + under, kPagePad + under};
}

void DividerTabs::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(tabBar() != nullptr);
  QPainter painter(this);
  const double under = kUnderSheets * kUnderStep;
  // The sheet starts at the open tab's foot, so the two are one piece
  // of paper whatever height the style gives the bar.
  const QTabBar* bar = tabBar();
  const int open = std::max(0, bar->currentIndex());
  const QRect open_tab = bar->tabRect(open).translated(bar->pos());
  const double top = open_tab.top() + open_tab.height() - kSheet;
  const QRectF sheet(0.0, top, width() - under, height() - top - under);
  for (int step = kUnderSheets; step > 0; --step) {
    const QRectF below = sheet.translated(step * kUnderStep,
                                          step * kUnderStep);
    const QColor paper = theme::kFace.darker(kFirstShut +
                                             kDarkerPerSheet * (step - 1));
    painter.fillRect(below, paper);
    painter.setPen(paper.darker(130));
    painter.drawRect(below.adjusted(0.0, 0.0, -1.0, -1.0));
  }
  // The open sheet's shadow on the ones under it.
  for (int ring = 1; ring <= kUnderSheets * 2; ++ring) {
    painter.fillRect(sheet.translated(ring * 0.75, ring * 0.75),
                     QColor(0, 0, 0, 22));
  }
  painter.fillRect(sheet, theme::kFace);
  // Lit cut edges at the top and left, but none where the open tab
  // joins: there the paper goes on unbroken.
  painter.setPen(theme::kFace.lighter(140));
  const bool has_tabs = bar->count() > 0;
  const double join_left = has_tabs ? open_tab.left() - kOverlap : 0.0;
  const double join_right = has_tabs ? open_tab.right() + kOverlap : 0.0;
  painter.drawLine(sheet.topLeft(), QPointF(join_left, top));
  painter.drawLine(QPointF(join_right, top), sheet.topRight());
  painter.drawLine(sheet.topLeft(), sheet.bottomLeft());
  painter.setPen(theme::kFace.darker(150));
  painter.drawLine(sheet.bottomLeft(), sheet.bottomRight());
  painter.drawLine(sheet.topRight(), sheet.bottomRight());
}

}  // namespace snapper
