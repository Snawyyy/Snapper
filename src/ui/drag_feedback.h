#ifndef SNAPPER_UI_DRAG_FEEDBACK_H_
#define SNAPPER_UI_DRAG_FEEDBACK_H_

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>

class QPainter;

namespace snapper {

class MotionManager;

// A pulse chip starts to fade this many milliseconds after the last
// Pulse.
inline constexpr int kRestMs = 700;

// What a drag shows besides the thing it drags: a value chip beside the
// cursor, a faint ghost where the drag began, a flash when it snaps,
// and a settle pop where it lets go. One of these serves each canvas
// (the stage, the rig canvas, the timelines), so they all talk the
// same way. It only draws; the canvas decides the words (drag_text.h)
// and when a drag starts, snaps, ends or is cancelled. Every movement
// goes through the Motion manager, so Off shows no flash and no pop.
class DragFeedback final : public QObject {
  Q_OBJECT

 public:
  DragFeedback(MotionManager* motion, QObject* parent);

  // Call on every move of a drag. The first call after Cancel or
  // Settle starts the ghost at start; later ones keep it there. A
  // snap (is_snapped, with new text) flashes.
  void Update(QPointF start, QPointF cursor, const QString& text,
              bool is_snapped);
  // The drag was let go: chip and ghost go, the pop plays at the cursor.
  void Settle();
  // Escape: everything vanishes at once, no pop.
  void Cancel();
  // A value with no drag behind it (the wheel turning a handle): the
  // chip shows beside the cursor, and fades out through Motion once
  // no Pulse has come for kRestMs. No ghost, no flash.
  void Pulse(QPointF cursor, const QString& text);

  bool is_active() const { return is_active_; }
  const QString& text() const { return text_; }
  QPointF ghost() const { return ghost_; }
  QPointF cursor() const { return cursor_; }
  // 1 as a snap flashes, easing to 0.
  double flash() const { return flash_; }
  // 1 while a pulse chip shows, easing to 0 as it fades; 0 when none.
  double pulse() const { return pulse_; }
  const QString& pulse_text() const { return pulse_text_; }
  // 1 as the pop starts, easing to exactly 0 when it is over (it may dip
  // below 0 on the way, the overshoot); 0 when none is playing.
  double settle() const { return settle_; }

  // Draws whatever is showing, inside bounds (the canvas rectangle).
  void Paint(QPainter* painter, const QRectF& bounds) const;

 signals:
  // Something to draw changed: repaint.
  void Changed();

 private:
  void StartFlash();
  void StartSettle();
  void PaintChip(QPainter* painter, const QRectF& bounds, QPointF cursor,
                 const QString& text) const;
  void StartFade();
  void PaintPop(QPainter* painter) const;

  MotionManager* motion_;
  bool is_active_ = false;
  QString text_;
  QPointF ghost_;
  QPointF cursor_;
  bool was_snapped_ = false;
  double flash_ = 0.0;
  double settle_ = 0.0;
  QPointF settle_at_;
  QString pulse_text_;
  QPointF pulse_at_;
  double pulse_ = 0.0;
  QTimer rest_timer_;
  QPointer<QVariantAnimation> pulse_animation_;
  QPointer<QVariantAnimation> flash_animation_;
  QPointer<QVariantAnimation> settle_animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_FEEDBACK_H_
