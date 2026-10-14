#ifndef SNAPPER_UI_COLOR_WHEEL_H_
#define SNAPPER_UI_COLOR_WHEEL_H_

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace snapper {

// A color wheel: a hue ring round a square of saturation (left to
// right) and brightness (bottom to top). Dragging in either moves its
// marker and previews the color in the middle of the ring's gap; the
// release picks it, so a drag is one pick. Decides nothing else.
class ColorWheel final : public QWidget {
  Q_OBJECT

 public:
  explicit ColorWheel(QWidget* parent);

  // Shows color, unless a drag is under way.
  void SetColor(const QColor& color);
  QSize sizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

 signals:
  void Picked(const QColor& color, Qt::KeyboardModifiers modifiers);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  enum class Part { kNone, kRing, kSquare };

  QPointF Middle() const;
  double Outer() const;
  double Inner() const;
  QRectF Square() const;
  Part PartAt(QPointF at) const;
  void DragTo(QPointF at);
  QColor Color() const;

  // Hue, saturation and value, 0 to 1.
  double hue_ = 0.0;
  double saturation_ = 0.0;
  double value_ = 0.0;
  Part dragging_ = Part::kNone;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COLOR_WHEEL_H_
