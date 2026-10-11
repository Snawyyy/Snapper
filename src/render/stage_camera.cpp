#include "render/stage_camera.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace snapper {
namespace {

double Limited(double zoom) {
  assert(kLeastStageZoom < kMostStageZoom);
  assert(std::isfinite(zoom));
  return std::clamp(zoom, kLeastStageZoom, kMostStageZoom);
}

}  // namespace

double FitScale(QSize canvas, QSize widget) {
  assert(canvas.width() > 0 && canvas.height() > 0);
  assert(widget.width() >= 0 && widget.height() >= 0);
  return kStageMargin *
         std::min(widget.width() / static_cast<double>(canvas.width()),
                  widget.height() / static_cast<double>(canvas.height()));
}

QPointF CornerOf(const StageCamera& camera, QSize canvas, QSize widget,
                 double scale) {
  assert(scale >= 0.0);
  assert(std::isfinite(camera.pan.x() + camera.pan.y()));
  return QPointF((widget.width() - canvas.width() * scale) / 2.0,
                 (widget.height() - canvas.height() * scale) / 2.0) +
         camera.pan;
}

StageCamera ZoomedAt(const StageCamera& camera, QPointF cursor, double factor,
                     QSize canvas, QSize widget) {
  assert(factor > 0.0 && std::isfinite(factor));
  assert(std::isfinite(cursor.x() + cursor.y()));
  const double fit = FitScale(canvas, widget);
  const double before = fit * camera.zoom;
  const QPointF corner = CornerOf(camera, canvas, widget, before);
  // The frame pixel under the cursor, which must stay there.
  const QPointF under = (cursor - corner) / std::max(before, 1e-9);
  StageCamera next = camera;
  next.zoom = Limited(camera.zoom * factor);
  const double after = fit * next.zoom;
  const QPointF centred = CornerOf({next.zoom, QPointF()}, canvas, widget,
                                   after);
  next.pan = cursor - under * after - centred;
  return next;
}

StageCamera Panned(const StageCamera& camera, QPointF delta) {
  assert(std::isfinite(delta.x() + delta.y()));
  assert(camera.zoom > 0.0);
  assert(std::isfinite(camera.pan.x() + camera.pan.y()));
  StageCamera next = camera;
  next.pan += delta;
  return next;
}

StageCamera Framing(const StageCamera& camera, const QRectF& box,
                    QSize canvas, QSize widget) {
  assert(canvas.width() > 0 && canvas.height() > 0);
  assert(widget.width() >= 0 && widget.height() >= 0);
  const bool is_empty = box.width() <= 0.0 && box.height() <= 0.0;
  if (is_empty) {
    return camera;
  }
  const double fit = FitScale(canvas, widget);
  const double before = fit * camera.zoom;
  const QPointF corner = CornerOf(camera, canvas, widget, before);
  // The box in frame pixels, then as big as fits.
  const QRectF frame((box.topLeft() - corner) / before,
                     box.size() / before);
  const double room = 0.8;
  const double across = frame.width() > 0.0
                            ? room * widget.width() / frame.width()
                            : std::numeric_limits<double>::max();
  const double down = frame.height() > 0.0
                          ? room * widget.height() / frame.height()
                          : std::numeric_limits<double>::max();
  StageCamera next;
  next.zoom = Limited(std::min(across, down) / fit);
  const double after = fit * next.zoom;
  const QPointF centred = CornerOf({next.zoom, QPointF()}, canvas, widget,
                                   after);
  next.pan = QPointF(widget.width(), widget.height()) / 2.0 -
             frame.center() * after - centred;
  return next;
}

StageCamera ActualSize(const StageCamera& camera, QSize canvas,
                       QSize widget) {
  assert(camera.zoom > 0.0);
  assert(widget.width() >= 0 && widget.height() >= 0);
  const double fit = FitScale(canvas, widget);
  StageCamera next;
  next.zoom = Limited(1.0 / fit);
  // Keep whatever is in the middle of the widget in the middle.
  const QPointF middle(widget.width() / 2.0, widget.height() / 2.0);
  const double before = fit * camera.zoom;
  const QPointF corner = CornerOf(camera, canvas, widget, before);
  const QPointF under = (middle - corner) / std::max(before, 1e-9);
  const double after = fit * next.zoom;
  const QPointF centred = CornerOf({next.zoom, QPointF()}, canvas, widget,
                                   after);
  next.pan = middle - under * after - centred;
  return next;
}

}  // namespace snapper
