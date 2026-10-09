#ifndef SNAPPER_UI_REFUSAL_SHAKER_H_
#define SNAPPER_UI_REFUSAL_SHAKER_H_

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

namespace snapper {

class MotionManager;

// How far a refused control sits from its place, in pixels, at progress
// 0 to 1 of the shake: it jerks left, right, left, right and settles,
// and is back at 0 at both ends. Pure, so the motion is tested without
// a widget.
double ShakeOffsetPx(double progress);

// The sideways travel of the first jerk, in pixels.
inline constexpr double kShakePx = 4.0;

// Why a greyed widget cannot act: its own tooltip, else the nearest
// parent's, else "Not available right now". Never empty.
QString RefusalReason(const QWidget* widget);

// A greyed control shakes when it is clicked anyway, and shows its
// reason (RefusalReason) beside the cursor at once. It
// watches the whole application, so no control has to opt in. The
// shake is timed by MotionManager (the Move time; none in Off, which
// still shows the reason).
class RefusalShaker final : public QObject {
  Q_OBJECT

 public:
  explicit RefusalShaker(MotionManager* motion);

  bool eventFilter(QObject* watched, QEvent* event) override;

  // Shakes widget once; a shake still running is put back first.
  void Shake(QWidget* widget);

 private:
  // Puts the shaking widget back where it was.
  void Settle();

  MotionManager* motion_;
  QPointer<QWidget> target_;
  QPoint origin_;
  QPointer<QVariantAnimation> animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REFUSAL_SHAKER_H_
