#ifndef SNAPPER_UI_HOVER_FADE_H_
#define SNAPPER_UI_HOVER_FADE_H_

#include <QObject>
#include <QPointer>
#include <QVariantAnimation>

#include "ui/motion_table.h"

class QEvent;
class QWidget;

namespace snapper {

class MotionManager;

// Gives a widget soft hover and press states instead of switches. It
// eases the widget's `kHoverProperty` between 0 and 1 over the Hover
// time as the pointer comes and goes, and its `kPressProperty` to 1
// over the Press time while a button is held and back with a spring
// over the Pop time on release. SnapperStyle paints with both. Nothing
// moves in Off. The widget owns it.
class HoverFade final : public QObject {
  Q_OBJECT

 public:
  HoverFade(QWidget* target, MotionManager* motion);

  bool eventFilter(QObject* watched, QEvent* event) override;
  // How far the hover has come, 0 to 1.
  double hover() const { return hover_.value; }
  // How far the press has come: 1 held, a little under 0 mid spring.
  double press() const { return press_.value; }

 private:
  // One eased number on the target: where it is and what moves it.
  struct Level {
    const char* property = nullptr;
    double value = 0.0;
    QPointer<QVariantAnimation> animation;
  };

  void EaseTo(Level* level, MotionKind kind, double to);

  QWidget* target_;
  MotionManager* motion_;
  Level hover_;
  Level press_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_HOVER_FADE_H_
