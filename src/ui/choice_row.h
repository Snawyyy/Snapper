#ifndef SNAPPER_UI_CHOICE_ROW_H_
#define SNAPPER_UI_CHOICE_ROW_H_

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QString>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include <vector>

namespace snapper {

// A few choices as a row of buttons, one pressed, in place of a drop-down
// box: every choice is on show and one click picks it. It has the parts
// of a combo box the dialogs use (addItem, currentIndex, currentData...).
class ChoiceRow final : public QWidget {
  Q_OBJECT

 public:
  explicit ChoiceRow(QWidget* parent = nullptr);

  void addItem(const QString& text, const QVariant& value = QVariant());
  int count() const { return static_cast<int>(buttons_.size()); }
  int currentIndex() const { return group_.checkedId(); }
  // Presses choice index (without anyone having clicked it).
  void setCurrentIndex(int index);
  void setItemText(int index, const QString& text);
  QVariant currentData() const;

 signals:
  void currentIndexChanged(int index);

 private:
  QHBoxLayout layout_;
  QButtonGroup group_;
  std::vector<QToolButton*> buttons_;
  std::vector<QVariant> data_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHOICE_ROW_H_
