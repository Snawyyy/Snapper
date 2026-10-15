#include "ui/flip_switch.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cassert>

#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kPlateWidth = 18.0;
constexpr double kLever = 9.0;
constexpr double kLamp = 3.0;
constexpr int kGap = 8;

}  // namespace

FlipSwitch::FlipSwitch(const QString& text) {
  assert(!text.isEmpty());
  assert(kPlateWidth > 0.0);
  setText(text);
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
}

QSize FlipSwitch::sizeHint() const {
  const int words = fontMetrics().horizontalAdvance(text());
  assert(words >= 0);
  return QSize(static_cast<int>(kPlateWidth) + 2 * kGap + words + 12,
               std::max(32, fontMetrics().height() + 8));
}

void FlipSwitch::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(height() > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const bool is_off_limits = !isEnabled();
  if (is_off_limits) {
    painter.setOpacity(0.45);
  }
  // The plate the lever stands in, screwed top and bottom.
  const QRectF plate(2.0, 2.0, kPlateWidth, height() - 4.0);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(deck::kPlasticLight);
  painter.drawRect(plate);
  deck::PaintScrew(&painter, QPointF(plate.center().x(), plate.top() + 4.0),
                   1.8);
  deck::PaintScrew(&painter,
                   QPointF(plate.center().x(), plate.bottom() - 4.0), 1.8);
  // The lever leans up while on, down while off, from its round nut.
  const QPointF nut = plate.center();
  const bool is_on = isChecked();
  const QPointF tip = nut + QPointF(0.0, is_on ? -kLever : kLever);
  QLinearGradient chrome(nut + QPointF(-3, 0), nut + QPointF(3, 0));
  chrome.setColorAt(0.0, QColor(0x9a, 0x94, 0x8e));
  chrome.setColorAt(0.5, QColor(0xf2, 0xee, 0xe8));
  chrome.setColorAt(1.0, QColor(0x6a, 0x64, 0x60));
  painter.setPen(QPen(QBrush(chrome), 4.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(nut, tip);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(QColor(0xc8, 0xc2, 0xba));
  painter.drawEllipse(nut, 3.5, 3.5);
  // The lamp and the name.
  const double text_left = plate.right() + kGap;
  painter.setPen(Qt::NoPen);
  painter.setBrush(is_on ? theme::kPick : theme::kShadow);
  painter.drawEllipse(QPointF(text_left + kLamp, height() / 2.0), kLamp,
                      kLamp);
  painter.setPen(is_on ? theme::kText : theme::kTextOff);
  painter.drawText(QRectF(text_left + 3 * kLamp + 4, 0, width(), height()),
                   Qt::AlignLeft | Qt::AlignVCenter, text());
}

}  // namespace snapper
