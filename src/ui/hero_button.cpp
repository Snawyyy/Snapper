#include "ui/hero_button.h"

#include <QPainter>

#include <cassert>
#include <cmath>

#include "ui/bevel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kBars = 7;
constexpr int kTickMs = 40;
constexpr int kBevel = 2;
constexpr double kHeightEms = 2.6;
constexpr double kTextGrow = 1.25;

}  // namespace

HeroButton::HeroButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {
  assert(!text.isEmpty());
  assert(kTickMs > 0);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QFont bigger = font();
  bigger.setBold(true);
  bigger.setPointSizeF(bigger.pointSizeF() * kTextGrow);
  setFont(bigger);
  tick_.setInterval(kTickMs);
  connect(&tick_, &QTimer::timeout, this, [this] {
    ++phase_;
    update();
  });
}

QSize HeroButton::sizeHint() const {
  const QSize plain = QPushButton::sizeHint();
  assert(plain.isValid());
  assert(kHeightEms > 1.0);
  return {plain.width(), static_cast<int>(theme::Em(this) * kHeightEms)};
}

void HeroButton::SetBusy(bool is_busy) {
  assert(tick_.interval() == kTickMs);
  assert(phase_ >= 0);
  const bool is_same = is_busy == tick_.isActive();
  if (is_same) {
    return;
  }
  if (is_busy) {
    tick_.start();
  } else {
    tick_.stop();
  }
  update();
}

void HeroButton::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(kBevel > 0);
  QPainter painter(this);
  const QRectF box = QRectF(rect()).adjusted(0, 0, -1, -1);
  const bool is_busy_now = is_busy();
  const bool can_act = isEnabled() && !is_busy_now;
  const bool is_sunk = isDown() || is_busy_now;
  QColor fill = theme::kFace;
  if (is_busy_now) {
    fill = theme::kPick.darker(250);
  } else if (can_act) {
    const bool is_hot = underMouse() && !isDown();
    fill = is_hot ? theme::kPick.lighter(115) : theme::kPick;
  }
  painter.fillRect(box, fill);
  DrawBevel(&painter, box, is_sunk, kBevel);
  QRectF words = box.adjusted(kBevel, kBevel, -kBevel, -kBevel);
  // A sunk button's face moves down and right, like a real key.
  const bool is_pressed = isDown();
  if (is_pressed) {
    words.translate(1, 1);
  }
  if (is_busy_now) {
    const double bars_width = words.height() * 1.2;
    PaintBars(&painter, QRectF(words.left() + words.height() * 0.4,
                               words.top(), bars_width, words.height()));
  }
  painter.setFont(font());
  painter.setPen(can_act || is_busy_now ? Qt::white : theme::kTextOff);
  painter.drawText(words, Qt::AlignCenter, text());
}

void HeroButton::PaintBars(QPainter* painter, const QRectF& area) const {
  assert(painter != nullptr);
  assert(kBars > 0);
  assert(area.width() >= 0.0);
  const double pitch = area.width() / kBars;
  for (int bar = 0; bar < kBars; ++bar) {
    // Each bar bounces at its own speed, so the row never repeats.
    const double wave =
        0.5 + 0.5 * std::sin(phase_ * (0.35 + 0.11 * bar) + bar * 1.7);
    const double height = area.height() * (0.2 + 0.6 * wave);
    const QRectF level(area.left() + bar * pitch + 1,
                       area.center().y() - height / 2.0, pitch - 2.0,
                       height);
    painter->fillRect(level, theme::kHandle);
  }
}

}  // namespace snapper
