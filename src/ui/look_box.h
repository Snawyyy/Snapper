#ifndef SNAPPER_UI_LOOK_BOX_H_
#define SNAPPER_UI_LOOK_BOX_H_

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include "model/doll.h"
#include "ui/flip_switch.h"
#include "ui/look_preview.h"
#include "ui/managers.h"
#include "ui/mode_knob.h"

namespace snapper {

// The Rig tab's Look box for the doll being rigged: a mode knob (Crisp,
// Smooth, Flat) picks how its drawings look when warped, turned or
// scaled, a small screen beside it shows that look on a turned pixel
// drawing, and the Hard edges switch cuts smooth edges out instead of
// blending them. Each click is one undo step through RigManager. A plated
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
  QHBoxLayout row_;
  ModeKnob knob_;
  LookPreview preview_;
  FlipSwitch hard_edges_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LOOK_BOX_H_
