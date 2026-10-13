#ifndef SNAPPER_UI_LOOK_BOX_H_
#define SNAPPER_UI_LOOK_BOX_H_

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

#include "model/doll.h"
#include "ui/managers.h"

namespace snapper {

// The Rig tab's Look box for the doll being rigged: three tiles (Crisp,
// Smooth, Smooth flat) pick how its drawings look when warped, turned
// or scaled, and Hard edges cuts smooth edges out instead of blending
// them. Each click is one undo step through RigManager. A plated
// section of its own (plated_box.h).
class LookBox final : public QWidget {
  Q_OBJECT

 public:
  explicit LookBox(const Managers& managers);

  void SetDoll(const QString& doll);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  Managers managers_;
  QString doll_;
  QVBoxLayout layout_;
  QHBoxLayout tiles_row_;
  QButtonGroup group_;
  std::array<QPushButton, kDrawingLookCount> tiles_;
  QCheckBox hard_edges_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LOOK_BOX_H_
