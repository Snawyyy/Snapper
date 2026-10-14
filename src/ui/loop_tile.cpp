#include "ui/loop_tile.h"

#include <QEnterEvent>
#include <QPainter>

#include <cassert>
#include <cmath>
#include <memory>

#include "ui/bevel.h"
#include "ui/material_art.h"
#include "ui/motion.h"
#include "ui/snapper_style.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kTileWidth = 64;
constexpr int kTileHeight = 62;
constexpr int kLabelHeight = 16;
constexpr int kFrameMs = 33;
// One loop of the cartoon.
constexpr int kLoopMs = 1200;

}  // namespace

LoopTile::LoopTile(const QString& label, MotionManager* motion,
                   QWidget* parent)
    : QAbstractButton(parent), motion_(motion), fade_(nullptr),
      phase_(kRestPhase) {
  assert(motion != nullptr);
  assert(!label.isEmpty());
  // The tile owns the fade as its Qt child.
  fade_ = std::make_unique<HoverFade>(this, motion).release();
  setText(label);
  setCursor(Qt::PointingHandCursor);
  timer_.setInterval(kFrameMs);
  connect(&timer_, &QTimer::timeout, this, [this] { Step(); });
}

QSize LoopTile::sizeHint() const { return {kTileWidth, kTileHeight}; }

void LoopTile::SetShare(Share share) {
  assert(static_cast<int>(share) >= 0);
  assert(fade_ != nullptr);
  const bool is_same = share == share_;
  if (is_same) {
    return;
  }
  share_ = share;
  update();
}

void LoopTile::Step() {
  assert(motion_ != nullptr);
  assert(kLoopMs > 0);
  phase_ += static_cast<double>(kFrameMs) / kLoopMs;
  phase_ -= std::floor(phase_);
  update();
}

void LoopTile::enterEvent(QEnterEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QAbstractButton::enterEvent(event);
  const bool is_moving =
      motion_->mode() != MotionMode::kOff && isEnabled();
  if (is_moving) {
    phase_ = 0.0;
    timer_.start();
  }
}

void LoopTile::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QAbstractButton::leaveEvent(event);
  timer_.stop();
  phase_ = kRestPhase;
  update();
}

void LoopTile::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(fade_ != nullptr);
  QPainter painter(this);
  const QRectF box = QRectF(rect()).adjusted(2, 2, -2, -2);
  const double scale = SquashScale(fade_->press());
  painter.translate(box.center());
  painter.scale(scale, scale);
  painter.translate(-box.center());
  const ButtonLook look =
      LookOfButton(isEnabled(), isDown(), false, fade_->hover());
  painter.fillRect(box, look.fill);
  DrawBevel(&painter, box, look.is_sunk);
  const QColor ink = isEnabled() ? theme::kText : theme::kTextOff;
  const QRectF art = box.adjusted(2, 2, -2, -kLabelHeight);
  PaintArt(&painter, art, phase_, ink);
  painter.setPen(ink);
  painter.drawText(QRectF(box.left(), box.bottom() - kLabelHeight,
                          box.width(), kLabelHeight),
                   Qt::AlignCenter, text());
  const bool is_used = share_ != Share::kNone;
  if (is_used) {
    const bool is_all = share_ == Share::kAll;
    painter.setPen(QPen(theme::kPick, is_all ? 3.0 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box.adjusted(1, 1, -1, -1));
  }
}

}  // namespace snapper
