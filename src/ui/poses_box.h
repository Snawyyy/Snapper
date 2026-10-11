#ifndef SNAPPER_UI_POSES_BOX_H_
#define SNAPPER_UI_POSES_BOX_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui/managers.h"

namespace snapper {

// Poses saved by name, for any project: save the picked doll's pose at
// the playhead, or key a saved one there.
class PosesBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit PosesBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  void Save();
  void Use();
  void Forget();
  QString Picked() const;

  Managers managers_;
  QVBoxLayout layout_;
  QListWidget list_;
  QLineEdit name_;
  QHBoxLayout buttons_;
  QPushButton save_;
  QPushButton use_;
  QPushButton forget_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POSES_BOX_H_
