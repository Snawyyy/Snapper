#ifndef SNAPPER_UI_MODE_KNOB_H_
#define SNAPPER_UI_MODE_KNOB_H_

#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace snapper {

// A hi-fi mode selector: a ridged knob whose pointer clicks between a
// few printed settings on an arc above it. A click on a setting's name
// turns to it; a click on the knob turns to the next; the wheel steps
// one either way. Picked says the user turned it.
class ModeKnob final : public QWidget {
  Q_OBJECT

 public:
  explicit ModeKnob(const QStringList& modes);

  int value() const { return value_; }
  // Turns to value without saying so.
  void SetValue(int value);

  QSize sizeHint() const override;

 signals:
  void Picked(int value);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  // Where setting i's name sits.
  QRectF LabelRect(int i) const;
  QRectF KnobRect() const;
  // The pointer's angle for setting i, in degrees from straight up.
  double AngleOf(int i) const;
  void Turn(int value);

  QStringList modes_;
  int value_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MODE_KNOB_H_
