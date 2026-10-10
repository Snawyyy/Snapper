#ifndef SNAPPER_UI_WOBBLE_BLOB_H_
#define SNAPPER_UI_WOBBLE_BLOB_H_

#include <QSize>
#include <QTimer>
#include <QWidget>

#include "anim/node_feel.h"
#include "ui/dot_kinds.h"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

namespace snapper {

class MotionManager;

// How far the preview has swung, -1 to 1, seconds after it was
// kicked, for a feel. Firm rings quicker; heavy swings wider and
// rings longer. The kick repeats every kWobbleLoopS, so it never rests.
inline constexpr double kWobbleLoopS = 2.4;
double WobbleAt(const Feel& feel, double seconds);

// A small live preview of the current feel: a jelly blob squashing, a
// dot on a spring (Drag) or a hanging sheet (Cloth) that wobbles the
// way the picked dots will. It runs only while shown and enabled and
// never in Off, where it shows the resting shape.
class WobbleBlob final : public QWidget {
  Q_OBJECT

 public:
  WobbleBlob(MotionManager* motion, QWidget* parent);

  void SetLook(Material material, const Feel& feel);
  bool is_animating() const { return timer_.isActive(); }
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  // Starts or stops the clock to match shown, enabled and Motion.
  void Sync();
  void Step();

  MotionManager* motion_;
  QTimer timer_;
  Material material_ = Material::kJelly;
  Feel feel_;
  double seconds_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_WOBBLE_BLOB_H_
