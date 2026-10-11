#ifndef SNAPPER_UI_STAGE_FLASH_H_
#define SNAPPER_UI_STAGE_FLASH_H_

#include <QObject>
#include <QPointer>
#include <QImage>
#include <QPointF>
#include <QVariantAnimation>

class QPainter;

namespace snapper {

class MotionManager;

// Shows undo and redo on the stage: what the step changed flashes white
// where it stands now, and a faint red copy fades where it stood, both
// shaped by the drawn pixels, not a box. It only draws, over the real
// frame, so nothing waits for it. It runs through MotionManager, so Off
// shows nothing.
class StageFlash final : public QObject {
  Q_OBJECT

 public:
  StageFlash(MotionManager* motion, QObject* parent);

  // Starts the flash from the changed pieces drawn on a clear picture,
  // as they stood before (was) and as they stand now, both at corner in
  // widget pixels. Replaces one still playing; two empty pictures show
  // nothing.
  void Show(const QImage& was, const QImage& now, QPointF corner);
  // Stops it at once.
  void Clear();

  // 1 as the flash starts, easing to 0; 0 when none is playing.
  double glow() const { return glow_; }
  bool IsPlaying() const { return glow_ > 0.0; }

  // Draws the flash over the frame, in widget pixels.
  void Paint(QPainter* painter) const;

 signals:
  // Something to draw changed: repaint.
  void Changed();

 private:
  MotionManager* motion_;
  // The pictures with every pixel gone solid white (now) or red (was).
  QImage was_;
  QImage now_;
  QPointF corner_;
  double glow_ = 0.0;
  QPointer<QVariantAnimation> animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STAGE_FLASH_H_
