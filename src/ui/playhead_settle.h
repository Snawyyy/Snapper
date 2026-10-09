#ifndef SNAPPER_UI_PLAYHEAD_SETTLE_H_
#define SNAPPER_UI_PLAYHEAD_SETTLE_H_

#include <QObject>
#include <QPointer>
#include <QVariantAnimation>

#include "base/frame.h"

namespace snapper {

class MotionManager;

// A jump longer than this just lands: a playhead flying across the
// whole song would be noise, not a settle.
inline constexpr int kMaxSettleFrames = 24;

// Lets a timeline's playhead settle on a frame it was sent to: it
// slides from where it was and overshoots a little, through
// MotionManager, so in Off it is on the frame at once. It only holds an
// offset to draw by; the real playhead is already on the frame.
class PlayheadSettle final : public QObject {
  Q_OBJECT

 public:
  PlayheadSettle(MotionManager* motion, Frame start, QObject* parent);

  // The playhead now belongs on to. While playing (every frame is a
  // move) or past kMaxSettleFrames it lands without settling.
  void MoveTo(Frame to, bool is_playing);
  // Frames to add to the playhead's place to draw it where it is
  // settling; 0 when it is at rest.
  double offset() const { return offset_; }

 signals:
  // The offset changed: repaint.
  void Changed();

 private:
  MotionManager* motion_;
  Frame target_;
  double offset_ = 0.0;
  QPointer<QVariantAnimation> animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_PLAYHEAD_SETTLE_H_
