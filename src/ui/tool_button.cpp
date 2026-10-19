#include "ui/tool_button.h"

#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kPadX = 10;
constexpr int kStrip = 3;

}  // namespace

ToolButton::ToolButton(const QString& name, const QString& key,
                       QWidget* parent)
    : QToolButton(parent), name_(name), key_(key) {
  assert(!name_.isEmpty());
  assert(key_.size() < 100);
  setCheckable(true);
  setFocusPolicy(Qt::NoFocus);
  setCursor(Qt::PointingHandCursor);
}

QSize ToolButton::sizeHint() const {
  const QFontMetrics metrics(font());
  QFont small = font();
  small.setPointSizeF(small.pointSizeF() * 0.8);
  const QFontMetrics small_metrics(small);
  const int width = std::max(metrics.horizontalAdvance(name_),
                             small_metrics.horizontalAdvance(key_));
  return {width + 2 * kPadX,
          metrics.height() + small_metrics.height() + kStrip + 8};
}

void ToolButton::enterEvent(QEnterEvent* event) {
  assert(event != nullptr);
  is_hovered_ = true;
  update();
  QToolButton::enterEvent(event);
}

void ToolButton::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  is_hovered_ = false;
  update();
  QToolButton::leaveEvent(event);
}

void ToolButton::paintEvent(QPaintEvent*) {
  assert(!name_.isEmpty());
  assert(width() >= 0);
  QPainter painter(this);
  const bool is_on = isChecked();
  const bool is_live = isEnabled();
  if (is_on) {
    painter.fillRect(rect(), theme::kFaceLight);
    painter.fillRect(QRect(0, height() - kStrip, width(), kStrip),
                     theme::kPick);
  } else if (is_hovered_ && is_live) {
    painter.fillRect(rect(), theme::kRaised.lighter(115));
  }
  QFont name_font = font();
  name_font.setBold(is_on);
  painter.setFont(name_font);
  painter.setPen(!is_live ? theme::kTextOff : theme::kText);
  const QFontMetrics metrics(name_font);
  const QRect top(0, 3, width(), metrics.height());
  painter.drawText(top, Qt::AlignHCenter | Qt::AlignVCenter, name_);
  QFont small = font();
  small.setPointSizeF(small.pointSizeF() * 0.8);
  painter.setFont(small);
  painter.setPen(is_live ? theme::kTextHint : theme::kTextOff);
  const QRect bottom(0, top.bottom() + 1, width(),
                     height() - top.bottom() - kStrip - 2);
  painter.drawText(bottom, Qt::AlignHCenter | Qt::AlignVCenter, key_);
}

}  // namespace snapper
