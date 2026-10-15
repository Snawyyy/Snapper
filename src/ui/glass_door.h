#ifndef SNAPPER_UI_GLASS_DOOR_H_
#define SNAPPER_UI_GLASS_DOOR_H_

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QTimer>

class QPainter;

namespace snapper {

// A glass sliding door over an opening, with a handle on its left edge.
// Dragging the handle slides it right into the wall and back; pulling
// up or down as it slides tips it a little on its slack, and it swings
// back level on a spring when let go. It keeps its own state and asks
// to be painted again through Changed.
class GlassDoor final : public QObject {
  Q_OBJECT

 public:
  GlassDoor();

  void Paint(QPainter* painter, const QRectF& opening) const;
  // Whether at lands on the handle (grabbed a little wider than drawn).
  bool IsOnHandle(const QPointF& at, const QRectF& opening) const;
  void Grab(const QPointF& at);
  void Drag(const QPointF& at, const QRectF& opening);
  void Release();
  // Puts the door back where the grab began.
  void Cancel();
  bool is_held() const { return is_held_; }

 signals:
  void Changed();

 private:
  // One step of the swing spring.
  void Swing();
  // The door's rect at its slide, before the tip.
  QRectF DoorRect(const QRectF& opening) const;
  QRectF HandleRect(const QRectF& door) const;

  // 0 closed over the opening, 1 slid fully into the wall.
  double open_ = 0.0;
  // Degrees tipped, the spin it swings with, and where it heads.
  double tilt_ = 0.0;
  double spin_ = 0.0;
  double tilt_aim_ = 0.0;
  bool is_held_ = false;
  QPointF grab_at_;
  double grab_open_ = 0.0;
  QTimer tick_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_GLASS_DOOR_H_
