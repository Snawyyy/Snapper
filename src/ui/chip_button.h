#ifndef SNAPPER_UI_CHIP_BUTTON_H_
#define SNAPPER_UI_CHIP_BUTTON_H_

#include <QPushButton>
#include <QSize>
#include <QString>

class QResizeEvent;

namespace snapper {

// A button that never makes its panel wider: it asks for room for its
// whole text, but can shrink to a few letters, showing the text cut with
// "..." (the tooltip should say the rest). Use it where names come from
// the user and a long one must not push a panel past its edge.
class ChipButton final : public QPushButton {
  Q_OBJECT

 public:
  explicit ChipButton(QWidget* parent);

  // The text in full; what shows is cut to fit.
  void SetFullText(const QString& text);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void Elide();

  QString full_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHIP_BUTTON_H_
