#ifndef SNAPPER_UI_LED_METER_H_
#define SNAPPER_UI_LED_METER_H_

#include <QTimer>
#include <QWidget>

class QPaintEvent;

namespace snapper {

// A recording meter like a tape deck's: a REC lamp that blinks, the
// seconds taken against the most allowed, and a row of square LEDs
// (green, then yellow, then red) lit to the level with a peak LED that
// holds a moment, then falls.
class LedMeter final : public QWidget {
  Q_OBJECT

 public:
  explicit LedMeter(QWidget* parent);

  // The loudest sample just heard, 0 to 1.
  void SetLevel(double peak);
  // Seconds taken so far, out of most.
  void SetSeconds(double seconds, double most);
  // Starts or stops the blinking; a stopped meter is dark.
  void SetRunning(bool is_running);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void Tick();
  void PaintLamp(QPainter* painter, const QRectF& lamp) const;
  void PaintLeds(QPainter* painter, const QRectF& row) const;

  QTimer tick_;
  double level_ = 0.0;
  double hold_ = 0.0;
  int hold_ticks_ = 0;
  int blink_ = 0;
  double seconds_ = 0.0;
  double most_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LED_METER_H_
