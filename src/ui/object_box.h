#ifndef SNAPPER_UI_OBJECT_BOX_H_
#define SNAPPER_UI_OBJECT_BOX_H_

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

#include <vector>

#include "model/layer.h"
#include "ui/managers.h"

namespace snapper {

// An object picked whole: how its GIF plays (Loop, Once, Ping-pong,
// Hold; speed; start; offset), what holds it frame by frame, and Drop
// here. The inspector shows it only for objects.
class ObjectBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit ObjectBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The picked object layers, the focused one first.
  std::vector<LayerId> Picked() const;
  void Commit();

  Managers managers_;
  QFormLayout layout_;
  QComboBox mode_;
  QSpinBox speed_;
  QSpinBox start_;
  QSpinBox offset_;
  QLabel holds_;
  QPushButton drop_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_OBJECT_BOX_H_
