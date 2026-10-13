#include "ui/view_camera.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "render/stage_camera.h"

namespace snapper {
namespace {

// Each wheel notch zooms by this much, as on the Pose stage.
constexpr double kZoomPerNotch = 1.15;
constexpr double kWheelNotch = 120.0;

QPointF Centred(QSizeF canvas, QSizeF widget, double scale) {
  assert(scale >= 0.0);
  assert(canvas.width() >= 0.0 && canvas.height() >= 0.0);
  return QPointF((widget.width() - canvas.width() * scale) / 2.0,
                 (widget.height() - canvas.height() * scale) / 2.0);
}

}  // namespace

ViewCamera::ViewCamera(QWidget* owner) : owner_(owner) {
  assert(owner_ != nullptr);
  assert(zoom_ > 0.0);
}

QPointF ViewCamera::CornerFor(double scale, QSizeF canvas,
                              QSizeF widget) const {
  assert(scale >= 0.0);
  assert(canvas.width() > 0.0 && canvas.height() > 0.0);
  return Centred(canvas, widget, scale) + pan_;
}

void ViewCamera::Reset() {
  assert(zoom_ > 0.0);
  zoom_ = 1.0;
  pan_ = QPointF();
  assert(pan_.isNull());
}

void ViewCamera::Wheel(const QWheelEvent& event, double base, QSizeF canvas,
                       QSizeF widget) {
  assert(base > 0.0);
  assert(canvas.width() > 0.0 && canvas.height() > 0.0);
  const double notches = event.angleDelta().y() / kWheelNotch;
  const bool is_turned = notches != 0.0;
  if (!is_turned) {
    return;
  }
  const double before = ScaleFor(base);
  const QPointF corner = CornerFor(before, canvas, widget);
  // The frame pixel under the pointer stays under it.
  const QPointF under = (event.position() - corner) / before;
  zoom_ = std::clamp(zoom_ * std::pow(kZoomPerNotch, notches),
                     kLeastStageZoom, kMostStageZoom);
  const double after = ScaleFor(base);
  pan_ = event.position() - under * after - Centred(canvas, widget, after);
}

bool ViewCamera::Press(const QMouseEvent& event) {
  assert(owner_ != nullptr);
  assert(zoom_ > 0.0);
  const bool is_pan = event.button() == Qt::MiddleButton;
  if (is_pan) {
    is_panning_ = true;
    pan_from_ = event.position();
    owner_->setCursor(Qt::ClosedHandCursor);
  }
  return is_pan;
}

bool ViewCamera::Move(const QMouseEvent& event) {
  assert(owner_ != nullptr);
  assert(std::isfinite(event.position().x() + event.position().y()));
  if (is_panning_) {
    pan_ += event.position() - pan_from_;
    pan_from_ = event.position();
  }
  return is_panning_;
}

bool ViewCamera::Release(const QMouseEvent& event) {
  assert(owner_ != nullptr);
  assert(zoom_ > 0.0);
  const bool was_pan = is_panning_ && event.button() == Qt::MiddleButton;
  if (was_pan) {
    is_panning_ = false;
    owner_->unsetCursor();
  }
  return was_pan;
}

}  // namespace snapper
