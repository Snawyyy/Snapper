#ifndef SNAPPER_UI_ON_SWITCH_H_
#define SNAPPER_UI_ON_SWITCH_H_

#include <QAbstractButton>
#include <QSize>
#include <QWidget>

class QPaintEvent;

namespace snapper {

// A square on switch, bigger than a check box so it is easy to hit: a
// sunken square, filled Teto red while on, lit while the mouse is over.
class OnSwitch final : public QAbstractButton {
 public:
  explicit OnSwitch(QWidget* parent);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_ON_SWITCH_H_
