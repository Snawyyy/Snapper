// RigCanvas's view: zoom toward the cursor and slide the view.

#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "render/stage_camera.h"
#include "ui/rig_canvas.h"

namespace snapper {
namespace {

// Each wheel notch zooms by this much, as on the stage.
constexpr double kZoomPerNotch = 1.15;
constexpr double kWheelNotch = 120.0;

}  // namespace

void RigCanvas::ZoomAt(QPointF cursor, double factor) {
  assert(std::isfinite(cursor.x() + cursor.y()));
  assert(factor > 0.0 && std::isfinite(factor));
  const double zoom =
      std::clamp(zoom_ * factor, kLeastStageZoom, kMostStageZoom);
  const QPointF middle(width() / 2.0, height() / 2.0);
  // The doll point under the cursor stays under it.
  pan_ = cursor - middle - (cursor - middle - pan_) * (zoom / zoom_);
  zoom_ = zoom;
  update();
}

void RigCanvas::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  const QPoint turn = event->angleDelta();
  const double notches = (turn.y() != 0 ? turn.y() : turn.x()) / kWheelNotch;
  if (notches != 0.0) {
    ZoomAt(event->position(), std::pow(kZoomPerNotch, notches));
  }
  event->accept();
}

bool RigCanvas::event(QEvent* event) {
  assert(event != nullptr);
  const bool is_gesture = event->type() == QEvent::NativeGesture;
  if (is_gesture) {
    const auto* gesture = static_cast<QNativeGestureEvent*>(event);
    const bool is_pinch = gesture->gestureType() == Qt::ZoomNativeGesture;
    if (is_pinch) {
      ZoomAt(gesture->position(), 1.0 + gesture->value());
      return true;
    }
  }
  const bool is_key = event->type() == QEvent::KeyPress;
  if (is_key) {
    const auto* key = static_cast<QKeyEvent*>(event);
    const bool is_fit = key->modifiers().testFlag(Qt::ShiftModifier) &&
                        (key->key() == Qt::Key_1 ||
                         key->key() == Qt::Key_Exclam);
    if (is_fit) {
      zoom_ = 1.0;
      pan_ = QPointF();
      update();
      return true;
    }
  }
  return QWidget::event(event);
}

bool RigCanvas::PanPress(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_pan = event->button() == Qt::MiddleButton;
  if (is_pan) {
    is_panning_ = true;
    pan_from_ = event->position();
    setCursor(Qt::ClosedHandCursor);
  }
  return is_pan;
}

bool RigCanvas::PanMove(QMouseEvent* event) {
  assert(event != nullptr);
  if (is_panning_) {
    pan_ += event->position() - pan_from_;
    pan_from_ = event->position();
    update();
  }
  return is_panning_;
}

bool RigCanvas::PanRelease(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_pan = is_panning_ && event->button() == Qt::MiddleButton;
  if (was_pan) {
    is_panning_ = false;
    unsetCursor();
  }
  return was_pan;
}

}  // namespace snapper
