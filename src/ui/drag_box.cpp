#include "ui/drag_box.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include "edit/rig_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {

DragBox::DragBox(const Managers& managers)
    : QGroupBox(tr("Drag node")), managers_(managers), layout_(this),
      live_(managers.history) {
  assert(managers_.IsComplete());
  drags_.setText(tr("Drags behind (D)"));
  drags_.setObjectName("drags");
  drags_.setToolTip(tr("The dot trails behind when the doll moves and "
                       "bounces back, like hair or a chest. Its rubber "
                       "reach spreads the wobble."));
  SetUpNumber(&lag_, Number::kFraction);
  SetUpNumber(&bounce_, Number::kFraction);
  lag_.setObjectName("lag");
  bounce_.setObjectName("bounce");
  lag_.setToolTip(tr("How far it trails behind: 0 keeps up, 1 drags "
                     "far."));
  bounce_.setToolTip(tr("How much it wobbles before settling: 0 settles "
                        "softly, 1 keeps bouncing."));
  layout_.addRow(QString(), &drags_);
  layout_.addRow(tr("Lag"), &lag_);
  layout_.addRow(tr("Bounce"), &bounce_);
  connect(&drags_, &QCheckBox::clicked, this, [this] {
    const auto picked = Pick();
    if (picked) {
      emit Problem(ProblemOf(managers_.rig->ToggleDragNode(
          picked->doll, picked->piece, picked->point)));
    }
  });
  for (QDoubleSpinBox* field : {&lag_, &bounce_}) {
    MakeLive(field, &live_, tr("Drag"), this, [this] { Commit(); });
  }
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 3);
}

std::optional<DragBox::Picked> DragBox::Pick() const {
  assert(managers_.selection != nullptr);
  const auto dot = PickedDotOf(managers_);
  if (!dot) {
    return std::nullopt;
  }
  const std::vector<DragNode>& nodes = dot->rig->drag_nodes;
  assert(nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  const auto node =
      std::find_if(nodes.begin(), nodes.end(),
                   [&dot](const DragNode& n) { return n.point == dot->point; });
  const bool is_drag = node != nodes.end();
  return Picked{dot->doll, dot->piece, dot->point,
                is_drag ? std::optional<DragNode>(*node) : std::nullopt};
}

void DragBox::Refresh() {
  assert(managers_.selection != nullptr);
  const auto picked = Pick();
  Explain(&drags_, picked ? QString()
                          : tr("Click a warp dot of a picked piece on the "
                               "stage first."));
  const bool is_drag = picked && picked->node.has_value();
  const QString why_not =
      !picked ? tr("Click a warp dot of a picked piece on the stage "
                   "first.")
              : tr("Tick \"Drags behind\" (or press D) first.");
  for (QDoubleSpinBox* field : {&lag_, &bounce_}) {
    Explain(field, is_drag ? QString() : why_not);
  }
  drags_.setChecked(is_drag);
  if (is_drag) {
    ShowNumber(&lag_, picked->node->lag);
    ShowNumber(&bounce_, picked->node->bounce);
  }
}

void DragBox::Commit() {
  const auto picked = Pick();
  const bool is_drag = picked && picked->node.has_value();
  if (!is_drag) {
    return;
  }
  DragNode node = *picked->node;
  node.lag = lag_.value();
  node.bounce = bounce_.value();
  emit Problem(
      ProblemOf(managers_.rig->SetDrag(picked->doll, picked->piece, node)));
  assert(managers_.rig != nullptr);
}

}  // namespace snapper
