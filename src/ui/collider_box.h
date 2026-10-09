#ifndef SNAPPER_UI_COLLIDER_BOX_H_
#define SNAPPER_UI_COLLIDER_BOX_H_

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

#include <memory>
#include <vector>

#include "ui/managers.h"

namespace snapper {

// The rig panel's list of what a collider pushes: every piece of the
// doll drawn in front of it, ticked while the collider pushes its
// cloth. Shown only while the focused piece is a collider.
class ColliderBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit ColliderBox(const Managers& managers);

  // Shows the collider of piece, or hides the box when it has none.
  void Show(const QString& doll, const QString& piece);

 signals:
  void Problem(const QString& why);

 private:
  Managers managers_;
  QString doll_;
  QString piece_;
  QVBoxLayout layout_;
  QLabel note_;
  std::vector<std::unique_ptr<QCheckBox>> ticks_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COLLIDER_BOX_H_
