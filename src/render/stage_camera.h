#ifndef SNAPPER_RENDER_STAGE_CAMERA_H_
#define SNAPPER_RENDER_STAGE_CAMERA_H_

#include <QPointF>
#include <QRectF>
#include <QSize>

namespace snapper {

// How the stage view looks at the frame: a zoom on top of the size that
// fits the whole frame in the widget, and a pan (widget pixels) that
// slides it. The doll and its colliders never feel it; it is only where
// the frame is drawn.
struct StageCamera final {
  // 1 fits the frame; 2 is twice as big.
  double zoom = 1.0;
  QPointF pan;

  bool operator==(const StageCamera&) const = default;
};

// The least and most the stage zooms in, over the fitting size.
inline constexpr double kLeastStageZoom = 0.25;
inline constexpr double kMostStageZoom = 4.0;
// Room left round the fitted frame so handles at its edge stay
// grabbable.
inline constexpr double kStageMargin = 0.94;

// Pixels per frame pixel when the frame fits widget.
double FitScale(QSize canvas, QSize widget);

// Where the frame's top left goes in the widget at scale, panned.
QPointF CornerOf(const StageCamera& camera, QSize canvas, QSize widget,
                 double scale);

// The camera zoomed by factor (kept within the limits) so the frame
// pixel under cursor (widget pixels) stays under it.
StageCamera ZoomedAt(const StageCamera& camera, QPointF cursor, double factor,
                     QSize canvas, QSize widget);

// The camera slid by delta widget pixels.
StageCamera Panned(const StageCamera& camera, QPointF delta);

// The camera that puts box (widget pixels, as seen through camera) in
// the middle of widget, as big as fits with some room round it, never
// past the zoom limits.
StageCamera Framing(const StageCamera& camera, const QRectF& box,
                    QSize canvas, QSize widget);

// 100 percent: one frame pixel per widget pixel, the middle kept.
StageCamera ActualSize(const StageCamera& camera, QSize canvas,
                       QSize widget);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_CAMERA_H_
