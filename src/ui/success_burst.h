#ifndef SNAPPER_UI_SUCCESS_BURST_H_
#define SNAPPER_UI_SUCCESS_BURST_H_

#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

namespace snapper {

class MotionManager;
class ToastManager;

// A rare, big thank-you: a burst of red pixels flies out of the bottom
// middle of the window when something big goes well (an export done, a
// rig saved). Full flies 24, Calm 10, and Off none. It floats over the
// window and never takes the mouse.
class SuccessBurst final : public QWidget {
  Q_OBJECT

 public:
  SuccessBurst(ToastManager* toasts, MotionManager* motion, QWidget* host);

  // Where the pixels are, 0 to 1; negative while none fly.
  double progress() const { return progress_; }
  // Flies the burst now.
  void Burst();

 protected:
  void paintEvent(QPaintEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  MotionManager* motion_;
  QWidget* host_;
  QPointer<QVariantAnimation> animation_;
  double progress_ = -1.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SUCCESS_BURST_H_
