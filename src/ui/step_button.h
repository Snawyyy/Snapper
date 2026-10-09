#ifndef SNAPPER_UI_STEP_BUTTON_H_
#define SNAPPER_UI_STEP_BUTTON_H_

#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVariantAnimation>

#include "ui/bar_text.h"

class QPaintEvent;

namespace snapper {

class MotionManager;

// The Undo or Redo button. It shows a StepFace, and when the words
// change the old ones slide up and out while the new ones slide in from
// below, through the Pop time (no slide in Off). text() is the new
// words at once, so the slide is only what is painted.
class StepButton final : public QPushButton {
  Q_OBJECT

 public:
  StepButton(const QString& verb, MotionManager* motion);

  void ShowFace(const StepFace& face);
  // 0 as the slide starts to 1 when it has landed.
  double slide() const { return slide_; }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  MotionManager* motion_;
  QPointer<QVariantAnimation> animation_;
  // What the button said before this slide, shown while it leaves.
  QString old_words_;
  QString words_;
  double slide_ = 1.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STEP_BUTTON_H_
