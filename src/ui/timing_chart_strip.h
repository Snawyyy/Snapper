#ifndef SNAPPER_UI_TIMING_CHART_STRIP_H_
#define SNAPPER_UI_TIMING_CHART_STRIP_H_

#include <QPointF>
#include <QWidget>

namespace snapper {

class TimingChartManager;

// The chart itself: a horizontal line from A to B with one tick for
// each frame in between and its frame number underneath. Drag a tick
// along the line; it can't cross its neighbours or pass A or B, and
// Escape puts it back. Hovering a tick, A or B shows that frame on the
// stage. It only draws and forwards; the manager holds the ticks.
class TimingChartStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit TimingChartStrip(TimingChartManager* chart);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  // The tick being dragged, or -1.
  int dragged() const { return dragged_; }
  // Where a tick of fraction sits, and the tick (or -1) at x.
  double XOf(double fraction) const;
  int TickAt(double x) const;

 signals:
  // The user moved a tick by hand, so no preset describes it now.
  void Edited();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  void PaintTicks(class QPainter* painter) const;
  void PaintEnds(class QPainter* painter) const;
  double FractionAt(double x) const;
  // The tick or end (-1 is A, count is B) near x, or -2 for none.
  int PartAt(double x) const;
  void Hover(double x);

  TimingChartManager* chart_;
  int dragged_ = -1;
  int hovered_ = -2;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMING_CHART_STRIP_H_
