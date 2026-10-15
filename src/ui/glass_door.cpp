#include "ui/glass_door.h"

#include <QLinearGradient>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kTickMs = 16;
// How far the door tips per pixel pulled up or down, and at most.
constexpr double kTiltPerPixel = 0.12;
constexpr double kMaxTilt = 1.0;
// The swing spring: pull toward the aim and the loss each step.
constexpr double kSwingPull = 0.18;
constexpr double kSwingKeep = 0.78;
constexpr double kRestTilt = 0.02;
// The door overlaps the opening's edges this far, as in its track.
constexpr double kOverlap = 2.0;
constexpr double kHandleWidth = 5.0;
constexpr double kHandleGrab = 9.0;

}  // namespace

GlassDoor::GlassDoor() {
  assert(kTickMs > 0);
  assert(kSwingKeep < 1.0);
  tick_.setInterval(kTickMs);
  connect(&tick_, &QTimer::timeout, this, &GlassDoor::Swing);
}

QRectF GlassDoor::DoorRect(const QRectF& opening) const {
  assert(opening.width() > 0.0);
  assert(open_ >= 0.0 && open_ <= 1.0);
  const QRectF door = opening.adjusted(-kOverlap, -kOverlap, kOverlap,
                                       kOverlap);
  return door.translated(open_ * opening.width(), 0.0);
}

QRectF GlassDoor::HandleRect(const QRectF& door) const {
  assert(door.width() > 0.0);
  assert(kHandleWidth > 0.0);
  const double tall = door.height() * 0.38;
  return QRectF(door.left() + 6.0, door.center().y() - tall / 2.0,
                kHandleWidth, tall);
}

void GlassDoor::Paint(QPainter* painter, const QRectF& opening) const {
  assert(painter != nullptr);
  assert(opening.width() > 0.0);
  const QRectF door = DoorRect(opening);
  painter->save();
  // It slides into the wall: nothing of it shows past the opening.
  painter->setClipRect(opening.adjusted(-kOverlap, -kOverlap, kOverlap,
                                        kOverlap));
  painter->translate(door.center());
  painter->rotate(tilt_);
  painter->translate(-door.center());
  painter->setPen(Qt::NoPen);
  // Smoked glass: it dims the cassette behind it, so the deck stays a
  // quiet part of the panel and the eye goes to the lists above.
  painter->setBrush(QColor(0x10, 0x0b, 0x0c, 120));
  painter->drawRect(door);
  // Two streaks of light across the glass.
  for (const double at : {0.18, 0.34}) {
    QLinearGradient streak(door.topLeft(), door.bottomRight());
    streak.setColorAt(std::max(0.0, at - 0.06), QColor(255, 255, 255, 0));
    streak.setColorAt(at, QColor(255, 255, 255, 46));
    streak.setColorAt(std::min(1.0, at + 0.06), QColor(255, 255, 255, 0));
    painter->setBrush(streak);
    painter->drawRect(door);
  }
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QColor(0xcf, 0xcc, 0xc9, 170), 1.5));
  painter->drawRect(door.adjusted(0.75, 0.75, -0.75, -0.75));
  const QRectF handle = HandleRect(door);
  QLinearGradient metal(handle.topLeft(), handle.topRight());
  metal.setColorAt(0.0, theme::kHandle.lighter(130));
  metal.setColorAt(1.0, theme::kHandle.darker(140));
  painter->setPen(QPen(theme::kHandle.darker(220), 1.0));
  painter->setBrush(metal);
  painter->drawRoundedRect(handle, 2.0, 2.0);
  painter->restore();
}

bool GlassDoor::IsOnHandle(const QPointF& at, const QRectF& opening) const {
  assert(opening.width() > 0.0);
  assert(kHandleGrab >= kHandleWidth);
  const QRectF handle = HandleRect(DoorRect(opening));
  const double spare = (kHandleGrab * 2.0 - handle.width()) / 2.0;
  return handle.adjusted(-spare, -spare, spare, spare).contains(at);
}

void GlassDoor::Grab(const QPointF& at) {
  assert(!is_held_);
  assert(open_ >= 0.0);
  is_held_ = true;
  grab_at_ = at;
  grab_open_ = open_;
  tick_.start();
}

void GlassDoor::Drag(const QPointF& at, const QRectF& opening) {
  assert(opening.width() > 0.0);
  assert(is_held_);
  const QPointF moved = at - grab_at_;
  open_ = std::clamp(grab_open_ + moved.x() / opening.width(), 0.0, 1.0);
  // Pulled up the handle side lifts, so the door tips the other way.
  tilt_aim_ =
      std::clamp(-moved.y() * kTiltPerPixel, -kMaxTilt, kMaxTilt);
  emit Changed();
}

void GlassDoor::Release() {
  assert(tick_.interval() == kTickMs);
  assert(is_held_ || !is_held_);
  is_held_ = false;
  tilt_aim_ = 0.0;
}

void GlassDoor::Cancel() {
  assert(is_held_);
  assert(grab_open_ >= 0.0);
  open_ = grab_open_;
  Release();
  emit Changed();
}

void GlassDoor::Swing() {
  assert(kSwingPull > 0.0);
  assert(kSwingKeep > 0.0);
  spin_ = (spin_ + (tilt_aim_ - tilt_) * kSwingPull) * kSwingKeep;
  tilt_ += spin_;
  // The track stops it hard: no swing past the limit.
  const bool is_at_stop = std::abs(tilt_) > kMaxTilt;
  if (is_at_stop) {
    tilt_ = std::clamp(tilt_, -kMaxTilt, kMaxTilt);
    spin_ = 0.0;
  }
  const bool is_settled = !is_held_ && std::abs(tilt_) < kRestTilt &&
                          std::abs(spin_) < kRestTilt;
  if (is_settled) {
    tilt_ = 0.0;
    spin_ = 0.0;
    tick_.stop();
  }
  emit Changed();
}

}  // namespace snapper
