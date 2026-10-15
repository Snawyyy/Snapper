#ifndef SNAPPER_UI_FLIP_SWITCH_H_
#define SNAPPER_UI_FLIP_SWITCH_H_

#include <QAbstractButton>
#include <QSize>
#include <QString>

class QPaintEvent;

namespace snapper {

// A toggle switch off an amplifier's face: a chrome bat lever in a
// screwed plate, up while on, down while off, its name printed beside
// it with a small red lamp that lights while on.
class FlipSwitch final : public QAbstractButton {
  Q_OBJECT

 public:
  explicit FlipSwitch(const QString& text);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FLIP_SWITCH_H_
