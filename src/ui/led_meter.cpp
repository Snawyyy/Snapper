#include "ui/led_meter.h"

#include <QFontDatabase>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "ui/bevel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr QColor kPanel{0x0c, 0x0c, 0x0e};
constexpr QColor kGreen{0x4c, 0xe0, 0x6c};
constexpr QColor kYellow{0xf2, 0xc2, 0x30};
constexpr QColor kRed{0xff, 0x3a, 0x4a};
constexpr int kLeds = 24;
// The share of the LEDs that are green, then green and yellow.
constexpr double kGreenTo = 0.66;
constexpr double kYellowTo = 0.86;
constexpr int kTickMs = 50;
// The lamp is lit for half of each blink, a blink is this many ticks.
constexpr int kBlinkTicks = 12;
constexpr int kHoldTicks = 16;
constexpr double kHoldFall = 0.02;
constexpr int kInset = 2;
constexpr double kHeightEms = 3.0;

QColor LedColor(int led) {
  assert(led >= 0 && led < kLeds);
  assert(kGreenTo < kYellowTo);
  const double at = static_cast<double>(led) / kLeds;
  return at < kGreenTo ? kGreen : at < kYellowTo ? kYellow : kRed;
}

}  // namespace

LedMeter::LedMeter(QWidget* parent) : QWidget(parent) {
  assert(kLeds > 0);
  assert(kTickMs > 0);
  setMinimumHeight(static_cast<int>(theme::Em(this) * kHeightEms));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  tick_.setInterval(kTickMs);
  connect(&tick_, &QTimer::timeout, this, &LedMeter::Tick);
}

void LedMeter::SetLevel(double peak) {
  assert(peak >= 0.0);
  assert(hold_ >= 0.0);
  level_ = std::clamp(peak, 0.0, 1.0);
  const bool is_new_peak = level_ >= hold_;
  if (is_new_peak) {
    hold_ = level_;
    hold_ticks_ = kHoldTicks;
  }
  update();
}

void LedMeter::SetSeconds(double seconds, double most) {
  assert(seconds >= 0.0);
  assert(most >= 0.0);
  seconds_ = seconds;
  most_ = most;
  update();
}

void LedMeter::SetRunning(bool is_running) {
  assert(tick_.interval() == kTickMs);
  assert(kBlinkTicks > 0);
  if (is_running) {
    blink_ = 0;
    tick_.start();
  } else {
    tick_.stop();
    level_ = 0.0;
    hold_ = 0.0;
  }
  update();
}

void LedMeter::Tick() {
  assert(kBlinkTicks > 0);
  assert(hold_ticks_ >= 0);
  blink_ = (blink_ + 1) % kBlinkTicks;
  hold_ticks_ = std::max(0, hold_ticks_ - 1);
  const bool is_falling = hold_ticks_ == 0;
  if (is_falling) {
    hold_ = std::max(level_, hold_ - kHoldFall);
  }
  update();
}

void LedMeter::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(kInset > 0);
  QPainter painter(this);
  const QRectF box = QRectF(rect()).adjusted(0, 0, -1, -1);
  painter.fillRect(box, kPanel);
  const QRectF inside = box.adjusted(kInset + 4, kInset + 3, -kInset - 4,
                                     -kInset - 3);
  const double top_height = inside.height() * 0.45;
  PaintLamp(&painter, QRectF(inside.topLeft(),
                             QSizeF(inside.width(), top_height)));
  PaintLeds(&painter, inside.adjusted(0, top_height + 3, 0, 0));
  DrawBevel(&painter, box, true, kInset);
}

void LedMeter::PaintLamp(QPainter* painter, const QRectF& lamp) const {
  assert(painter != nullptr);
  assert(kBlinkTicks > 1);
  const bool is_lit = tick_.isActive() && blink_ < kBlinkTicks / 2;
  const double side = lamp.height() * 0.7;
  const QRectF light(lamp.left(), lamp.center().y() - side / 2.0, side,
                     side);
  painter->fillRect(light, is_lit ? kRed : kRed.darker(320));
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setBold(true);
  painter->setFont(mono);
  painter->setPen(is_lit ? kRed : kRed.darker(200));
  const QRectF words = lamp.adjusted(side + 6, 0, 0, 0);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignVCenter,
                    QStringLiteral("REC"));
  painter->setPen(kGreen);
  painter->drawText(words, Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("%1 / %2 s")
                        .arg(seconds_, 0, 'f', 1)
                        .arg(most_, 0, 'f', 0));
}

void LedMeter::PaintLeds(QPainter* painter, const QRectF& row) const {
  assert(painter != nullptr);
  assert(kLeds > 0);
  const double pitch = row.width() / kLeds;
  const int lit = static_cast<int>(level_ * kLeds + 0.5);
  const int held = std::min(kLeds - 1, static_cast<int>(hold_ * kLeds));
  for (int led = 0; led < kLeds; ++led) {
    const QRectF square(row.left() + led * pitch + 1, row.top(),
                        std::max(1.0, pitch - 2.0), row.height());
    const bool is_on = led < lit || (hold_ > 0.0 && led == held);
    const QColor color = LedColor(led);
    painter->fillRect(square, is_on ? color : color.darker(400));
  }
}

}  // namespace snapper
