#ifndef SNAPPER_UI_DRAG_BOX_H_
#define SNAPPER_UI_DRAG_BOX_H_

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QString>

#include <array>
#include <optional>
#include <vector>

#include "edit/rig_manager.h"
#include "model/doll.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The warp dots picked on the stage: whether they drag behind (as D
// does) and, when they do, their lag and bounce, or, with Physics
// ticked, the five physics settings (stretch, inertia, air resistance,
// gravity, bounce) instead. Every picked dot is edited at once: a
// field whose numbers differ shows Mixed, a tick that is on for some
// shows partly ticked, and a change sets them all as one undo step.
// Greyed out with the reason when no dot is picked.
class DragBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DragBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The picked dots, and the drag nodes among them.
  struct Look final {
    std::vector<DotRef> dots;
    std::vector<const DragNode*> nodes;
  };
  Look LookAtPick() const;
  void Commit(DragField field, const QDoubleSpinBox* box);
  void SetUpPhysicsRows();
  // The field that sets which number, in AllFields() order.
  static DragField FieldAt(size_t index);
  std::array<QDoubleSpinBox*, 7> AllFields();

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QCheckBox drags_;
  QDoubleSpinBox lag_;
  QDoubleSpinBox bounce_;
  QCheckBox physics_;
  QPushButton mirror_;
  QDoubleSpinBox stretch_;
  QDoubleSpinBox inertia_;
  QDoubleSpinBox air_;
  QDoubleSpinBox gravity_;
  QDoubleSpinBox spring_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_BOX_H_
