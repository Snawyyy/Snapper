#ifndef SNAPPER_UI_HERO_BUTTON_H_
#define SNAPPER_UI_HERO_BUTTON_H_

#include <QPushButton>
#include <QTimer>

class QPaintEvent;

namespace snapper {

// The one big button of a panel, the thing it is there to do: tall,
// raised, Teto red with bold text when it can act, a plain face when it
// can't. While busy it sinks to a dark red and bounces a row of
// equalizer bars beside its text, so waiting looks alive.
class HeroButton final : public QPushButton {
  Q_OBJECT

 public:
  HeroButton(const QString& text, QWidget* parent);

  void SetBusy(bool is_busy);
  bool is_busy() const { return tick_.isActive(); }
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void PaintBars(QPainter* painter, const QRectF& area) const;

  QTimer tick_;
  int phase_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_HERO_BUTTON_H_
