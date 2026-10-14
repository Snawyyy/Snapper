// StageView's camera: zoom toward the cursor, pan, fit and frame.

#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QWheelEvent>

#include <cassert>
#include <cmath>
#include <vector>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_hit.h"
#include "ui/drag_text.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// Each wheel notch zooms by this much.
constexpr double kZoomPerNotch = 1.15;
constexpr double kWheelNotch = 120.0;

}  // namespace

void StageView::SetCamera(const StageCamera& camera) {
  assert(camera.zoom > 0.0);
  assert(std::isfinite(camera.pan.x() + camera.pan.y()));
  camera_ = camera;
  update();
}

void StageView::ZoomBy(double notches, QPointF cursor) {
  assert(std::isfinite(notches));
  assert(std::isfinite(cursor.x() + cursor.y()));
  const CanvasSize& canvas = managers_.history->current().canvas;
  assert(canvas.width > 0);
  camera_ = ZoomedAt(camera_, cursor, std::pow(kZoomPerNotch, notches),
                     QSize(canvas.width, canvas.height), size());
  ShowZoom(cursor);
  update();
}

void StageView::ShowZoom(QPointF cursor) {
  assert(std::isfinite(cursor.x() + cursor.y()));
  assert(managers_.history != nullptr);
  const auto frame = CurrentFrame();
  const bool has_frame = frame.has_value();
  if (has_frame) {
    const long percent = std::lround(frame->scale * 100.0);
    feedback_.Pulse(cursor, QStringLiteral("%1%").arg(percent));
  }
}

QRectF StageView::PickBox(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  return BoxOf(frame, managers_.selection->PickedLayers());
}

QRectF StageView::AllBox(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const Shot* shot = FindShot(managers_.history->current(), frame.shot);
  const CanvasSize& canvas = managers_.history->current().canvas;
  const QRectF inside(frame.corner, QSizeF(canvas.width * frame.scale,
                                           canvas.height * frame.scale));
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return inside;
  }
  std::vector<LayerId> layers;
  for (const Layer& layer : shot->layers) {
    const bool is_live = IsLayerLive(layer, frame.local, shot->length);
    if (is_live) {
      layers.push_back(layer.id);
    }
  }
  const QRectF things = BoxOf(frame, layers);
  return things.isNull() ? inside : inside.united(things);
}

QRectF StageView::BoxOf(const StageFrame& frame,
                        const std::vector<LayerId>& layers) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  QRectF box;
  const bool has_shot = shot != nullptr;
  for (const LayerId layer : layers) {
    const QRectF shape =
        has_shot ? LayerShape(project, *shot, layer, frame.local, frame.scale,
                              renderer_.cache())
                       .boundingRect()
                       .translated(frame.corner)
                 : QRectF();
    const bool is_shown = !shape.isNull();
    if (is_shown) {
      box = box.isNull() ? shape : box.united(shape);
    }
  }
  return box;
}

bool StageView::CameraKey(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier) &&
                        !event->modifiers().testFlag(Qt::ControlModifier);
  const int key = event->key();
  // Shift turns the digits into their symbols on most keyboards.
  const bool is_fit = key == Qt::Key_1 || key == Qt::Key_Exclam;
  const bool is_pick = key == Qt::Key_2 || key == Qt::Key_At;
  const bool is_all = key == Qt::Key_3 || key == Qt::Key_NumberSign;
  const bool is_actual = key == Qt::Key_0 || key == Qt::Key_ParenRight;
  const auto frame = CurrentFrame();
  const bool is_camera = is_shift && frame.has_value() &&
                         (is_fit || is_pick || is_actual || is_all);
  if (!is_camera) {
    return false;
  }
  const CanvasSize& canvas = managers_.history->current().canvas;
  const QSize canvas_size(canvas.width, canvas.height);
  if (is_fit) {
    camera_ = StageCamera();
  } else if (is_all) {
    camera_ = Framing(camera_, AllBox(*frame), canvas_size, size());
  } else if (is_actual) {
    camera_ = ActualSize(camera_, canvas_size, size());
  } else {
    camera_ = Framing(camera_, PickBox(*frame), canvas_size, size());
  }
  ShowZoom(QPointF(width() / 2.0, height() / 2.0));
  update();
  return true;
}

bool StageView::event(QEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const bool is_gesture = event->type() == QEvent::NativeGesture;
  if (is_gesture) {
    const auto* gesture = static_cast<QNativeGestureEvent*>(event);
    const bool is_pinch = gesture->gestureType() == Qt::ZoomNativeGesture;
    if (is_pinch) {
      ZoomBy(std::log(1.0 + gesture->value()) / std::log(kZoomPerNotch),
             gesture->position());
      return true;
    }
  }
  const bool is_override = event->type() == QEvent::ShortcutOverride;
  if (is_override) {
    const int key = static_cast<QKeyEvent*>(event)->key();
    const bool is_arrow = key == Qt::Key_Left || key == Qt::Key_Right ||
                          key == Qt::Key_Up || key == Qt::Key_Down;
    // Plain arrows step frames; Shift ones nudge the pick.
    const bool is_shift = static_cast<QKeyEvent*>(event)->modifiers().testFlag(
        Qt::ShiftModifier);
    const bool is_ours =
        is_arrow && is_shift && !managers_.selection->picks().empty();
    if (is_ours) {
      event->accept();
      return true;
    }
  }
  return QWidget::event(event);
}

bool StageView::NudgeKey(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const int key = event->key();
  const bool is_arrow = key == Qt::Key_Left || key == Qt::Key_Right ||
                        key == Qt::Key_Up || key == Qt::Key_Down;
  const auto frame = CurrentFrame();
  // Shift+arrow nudges a pixel, Ctrl+Shift ten; plain arrows step frames.
  const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier);
  const bool is_nudge = is_arrow && is_shift && frame.has_value() &&
                        !managers_.selection->picks().empty();
  if (!is_nudge) {
    return false;
  }
  const double step =
      event->modifiers().testFlag(Qt::ControlModifier) ? 10.0 : 1.0;
  const QPointF way(key == Qt::Key_Left ? -1.0 : key == Qt::Key_Right ? 1.0
                                                                      : 0.0,
                    key == Qt::Key_Up ? -1.0 : key == Qt::Key_Down ? 1.0
                                                                   : 0.0);
  tool_.Nudge(way * step, *frame);
  ReportTool();
  update();
  return true;
}

bool StageView::focusNextPrevChild(bool next) {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const bool has_pick = !managers_.selection->picks().empty();
  if (!has_pick) {
    return QWidget::focusNextPrevChild(next);
  }
  managers_.selection->StepPick(next ? 1 : -1);
  update();
  return true;
}

}  // namespace snapper
