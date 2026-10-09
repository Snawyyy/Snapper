#ifndef SNAPPER_UI_NUMBER_INPUT_H_
#define SNAPPER_UI_NUMBER_INPUT_H_

#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QObject>
#include <QString>

namespace snapper {

// Lets a number field take math. Type "*2", "+15" or "90/2" and press
// Enter (or leave the field) and the result lands, using EvalNumber.
// Text it cannot read leaves the field as it was and says so: the
// field flashes red and a tip names what was typed. It never touches
// a field that was not typed in, so a refresh is never fought.
//
// Lives as a child of the field, which owns it.
class NumberInput final : public QObject {
  Q_OBJECT

 public:
  // Gives box a NumberInput; does nothing twice on the same box.
  static void Install(QAbstractSpinBox* box);

  // Owned by box, which must outlive it: use Install.
  explicit NumberInput(QAbstractSpinBox* box);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void Commit();
  void Refuse(const QString& typed);
  QString Body() const;

  QAbstractSpinBox* box_;
  QLineEdit* edit_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_NUMBER_INPUT_H_
