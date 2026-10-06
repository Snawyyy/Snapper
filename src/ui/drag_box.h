#ifndef SNAPPER_UI_DRAG_BOX_H_
#define SNAPPER_UI_DRAG_BOX_H_

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QString>

#include <optional>

#include "model/doll.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The warp dot picked on the stage: whether it drags behind (as D does)
// and, when it does, its lag and bounce. Greyed out with the reason
// when no dot is picked.
class DragBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DragBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The picked dot's doll name and drag node, if it is one.
  struct Picked final {
    QString doll;
    QString piece;
    int point = -1;
    std::optional<DragNode> node;
  };
  std::optional<Picked> Pick() const;
  void Commit();

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QCheckBox drags_;
  QDoubleSpinBox lag_;
  QDoubleSpinBox bounce_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_BOX_H_
