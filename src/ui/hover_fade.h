#ifndef SNAPPER_UI_HOVER_FADE_H_
#define SNAPPER_UI_HOVER_FADE_H_

#include <QObject>
#include <QPointer>
#include <QVariantAnimation>

class QEvent;
class QWidget;

namespace snapper {

class MotionManager;

// Makes a widget's hover fade in and out instead of switching: it eases
// the widget's `kHoverProperty` between 0 and 1 over the Hover time (at
// once in Off), and SnapperStyle paints with it. The widget owns it.
class HoverFade final : public QObject {
  Q_OBJECT

 public:
  HoverFade(QWidget* target, MotionManager* motion);

  bool eventFilter(QObject* watched, QEvent* event) override;
  // How far the hover has come, 0 to 1.
  double level() const { return level_; }

 private:
  void FadeTo(double to);

  QWidget* target_;
  MotionManager* motion_;
  QPointer<QVariantAnimation> animation_;
  double level_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_HOVER_FADE_H_
