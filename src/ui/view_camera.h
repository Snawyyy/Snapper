#ifndef SNAPPER_UI_VIEW_CAMERA_H_
#define SNAPPER_UI_VIEW_CAMERA_H_

#include <QPointF>
#include <QSizeF>

class QMouseEvent;
class QWheelEvent;
class QWidget;

namespace snapper {

// Zoom and pan for a preview that draws the frame fitted in its widget:
// the wheel zooms toward the pointer, a middle button drag grabs and
// moves the frame, Shift+1 puts it back. The owner keeps its own fit
// (the scale that shows the whole frame); the camera is the zoom on top
// of it and the slide (widget pixels) from the middle.
class ViewCamera final {
 public:
  explicit ViewCamera(QWidget* owner);

  // The zoom over the fit (1 fits, 2 is twice as big), the slide, and
  // the scale for a fit of base.
  double zoom() const { return zoom_; }
  const QPointF& pan() const { return pan_; }
  double ScaleFor(double base) const { return base * zoom_; }
  // The frame's top left centred in widget at scale, plus the slide.
  QPointF CornerFor(double scale, QSizeF canvas, QSizeF widget) const;

  // Back to the whole frame in the middle.
  void Reset();
  // One wheel event: zooms toward its position. Always taken.
  void Wheel(const QWheelEvent& event, double base, QSizeF canvas,
             QSizeF widget);
  // A middle button drag; each returns whether the event was the
  // camera's (a drag in progress or the middle button itself).
  bool Press(const QMouseEvent& event);
  bool Move(const QMouseEvent& event);
  bool Release(const QMouseEvent& event);

 private:
  QWidget* owner_;
  double zoom_ = 1.0;
  QPointF pan_;
  bool is_panning_ = false;
  QPointF pan_from_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VIEW_CAMERA_H_
