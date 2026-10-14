#include "ui/timeline_scale.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// One mouse wheel notch, in the angle delta Qt reports.
constexpr double kWheelUnit = 120.0;

}  // namespace

TimelineScale::TimelineScale(const QWidget* view, double left,
                             double pixels_per_frame)
    : view_(view), left_(left), pixels_per_frame_(pixels_per_frame) {
  assert(view_ != nullptr);
  assert(pixels_per_frame_ > 0.0);
  pixels_per_frame_ = std::clamp(pixels_per_frame_, kMinPixelsPerFrame,
                                 kMaxPixelsPerFrame);
}

double TimelineScale::ViewWidth() const {
  assert(view_ != nullptr);
  assert(left_ >= 0.0);
  return std::max(1.0, view_->width() - left_ - kFitMargin);
}

double TimelineScale::pixels_per_frame() const {
  assert(length_ >= 1);
  assert(pixels_per_frame_ > 0.0);
  if (!is_fit_) {
    return pixels_per_frame_;
  }
  return std::clamp(ViewWidth() / length_, kMinPixelsPerFrame,
                    kMaxPixelsPerFrame);
}

double TimelineScale::first() const {
  assert(first_ >= 0.0);
  assert(length_ >= 1);
  return is_fit_ ? 0.0 : first_;
}

double TimelineScale::VisibleFrames() const {
  assert(pixels_per_frame() > 0.0);
  assert(ViewWidth() >= 1.0);
  return ViewWidth() / pixels_per_frame();
}

double TimelineScale::XOf(double frame) const {
  assert(std::isfinite(frame));
  assert(pixels_per_frame() > 0.0);
  return left_ + (frame - first()) * pixels_per_frame();
}

double TimelineScale::FrameAtX(double x) const {
  assert(std::isfinite(x));
  assert(pixels_per_frame() > 0.0);
  return first() + (x - left_) / pixels_per_frame();
}

int TimelineScale::FrameAt(double x) const {
  assert(std::isfinite(x));
  assert(pixels_per_frame() > 0.0);
  return static_cast<int>(std::floor(FrameAtX(x)));
}

void TimelineScale::ClampFirst() {
  assert(length_ >= 1);
  assert(std::isfinite(first_));
  first_ = std::clamp(first_, 0.0, static_cast<double>(length_ - 1));
}

void TimelineScale::SetLength(int frames) {
  assert(frames >= 0);
  assert(frames < 100000000);
  length_ = std::max(1, frames);
  ClampFirst();
}

void TimelineScale::Detach() {
  assert(pixels_per_frame() > 0.0);
  assert(first_ >= 0.0);
  const bool was_fit = is_fit_;
  if (was_fit) {
    pixels_per_frame_ = pixels_per_frame();
    first_ = 0.0;
    is_fit_ = false;
  }
  assert(!is_fit_);
}

void TimelineScale::SetPixelsPerFrame(double pixels_per_frame) {
  assert(std::isfinite(pixels_per_frame));
  assert(pixels_per_frame > 0.0);
  Detach();
  pixels_per_frame_ = std::clamp(pixels_per_frame, kMinPixelsPerFrame,
                                 kMaxPixelsPerFrame);
}

void TimelineScale::ZoomAround(double factor, double x) {
  assert(factor > 0.0);
  assert(std::isfinite(x));
  Detach();
  const double right = std::max<double>(left_, view_->width());
  const double at = std::clamp(x, left_, right);
  const double under = FrameAtX(at);
  SetPixelsPerFrame(pixels_per_frame_ * factor);
  first_ = under - (at - left_) / pixels_per_frame_;
  ClampFirst();
}

void TimelineScale::ZoomAroundFrame(double factor, double frame) {
  assert(factor > 0.0);
  assert(std::isfinite(frame));
  ZoomAround(factor, XOf(frame));
}

void TimelineScale::ScrollBy(double frames) {
  assert(std::isfinite(frames));
  assert(length_ >= 1);
  Detach();
  first_ += frames;
  ClampFirst();
  is_following_ = false;
}

void TimelineScale::FitAll() {
  assert(length_ >= 1);
  assert(pixels_per_frame_ > 0.0);
  is_fit_ = true;
  first_ = 0.0;
}

double TimelineScale::Notches(const QWheelEvent& event) {
  const QPoint turn = event.angleDelta();
  assert(kWheelUnit > 0.0);
  const bool is_vertical = turn.y() != 0;
  const double notches = (is_vertical ? turn.y() : turn.x()) / kWheelUnit;
  assert(std::isfinite(notches));
  return notches;
}

WheelUse TimelineScale::Wheel(const QWheelEvent& event) {
  const double notches = Notches(event);
  const Qt::KeyboardModifiers mods = event.modifiers();
  const bool is_zoom = mods.testFlag(Qt::ControlModifier);
  const bool is_rows = mods.testFlag(Qt::ShiftModifier);
  assert(length_ >= 1);
  assert(std::isfinite(notches));
  if (is_zoom) {
    ZoomAround(std::pow(kZoomStep, notches), event.position().x());
    return WheelUse::kZoomed;
  }
  if (is_rows) {
    return WheelUse::kRows;
  }
  // Wheel up goes back in time.
  ScrollBy(-notches * VisibleFrames() * kScrollShare);
  return WheelUse::kScrolled;
}

bool TimelineScale::Follow(double frame) {
  assert(std::isfinite(frame));
  assert(length_ >= 1);
  const bool can_page = is_following_ && !is_fit_;
  if (!can_page) {
    return false;
  }
  const bool is_in_view =
      frame >= first_ && frame < first_ + VisibleFrames();
  if (is_in_view) {
    return false;
  }
  first_ = frame;
  ClampFirst();
  return true;
}

}  // namespace snapper
