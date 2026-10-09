#include "ui/drag_box.h"

#include <algorithm>
#include <array>
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
  AddNumberRow(&layout_, tr("Lag"), &lag_, &live_);
  AddNumberRow(&layout_, tr("Bounce"), &bounce_, &live_);
  SetUpPhysicsRows();
  connect(&drags_, &QCheckBox::clicked, this, [this] {
    const auto picked = Pick();
    if (picked) {
      emit Problem(ProblemOf(managers_.rig->ToggleDragNode(
          picked->doll, picked->piece, picked->point)));
    }
  });
  connect(&physics_, &QCheckBox::clicked, this, [this](bool on) {
    const auto picked = Pick();
    if (picked) {
      emit Problem(ProblemOf(managers_.rig->SetPhysics(
          picked->doll, picked->piece, picked->point, on)));
    }
  });
  for (QDoubleSpinBox* field : AllFields()) {
    MakeLive(field, &live_, tr("Drag"), this, [this] { Commit(); });
  }
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 9);
}

std::array<QDoubleSpinBox*, 7> DragBox::AllFields() {
  assert(layout_.rowCount() >= 3);
  assert(managers_.IsComplete());
  return {&lag_, &bounce_, &stretch_, &inertia_, &air_, &gravity_, &spring_};
}

void DragBox::SetUpPhysicsRows() {
  assert(layout_.rowCount() == 3);
  assert(managers_.IsComplete());
  physics_.setText(tr("Physics"));
  physics_.setObjectName("physics");
  physics_.setToolTip(tr("Turns the dot into cloth or soft flesh: dots "
                         "next to each other hang together, and the "
                         "other dots of the grid hold them up."));
  layout_.addRow(QString(), &physics_);
  const struct Row final {
    const char* name;
    QDoubleSpinBox* field;
    const char* label;
    const char* tip;
  } rows[] = {
      {"stretch", &stretch_, QT_TR_NOOP("Stretch"),
       QT_TR_NOOP("0 keeps the cloth its length between linked dots, 1 "
                  "is rubbery.")},
      {"inertia", &inertia_, QT_TR_NOOP("Inertia"),
       QT_TR_NOOP("How much it keeps moving when the doll stops or "
                  "turns.")},
      {"air", &air_, QT_TR_NOOP("Air resistance"),
       QT_TR_NOOP("How fast its motion dies down.")},
      {"gravity", &gravity_, QT_TR_NOOP("Gravity"),
       QT_TR_NOOP("A steady pull down, so cloth hangs and sags.")},
      {"spring", &spring_, QT_TR_NOOP("Bounce"),
       QT_TR_NOOP("A spring back to the drawn shape: 0 none (cloth), 1 "
                  "very springy.")}};
  for (const Row& row : rows) {
    SetUpNumber(row.field, Number::kFraction);
    row.field->setObjectName(row.name);
    row.field->setToolTip(tr(row.tip));
    AddNumberRow(&layout_, tr(row.label), row.field, &live_);
  }
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
  assert(managers_.IsComplete());
  const auto picked = Pick();
  Explain(&drags_, picked ? QString()
                          : tr("Click a warp dot of a picked piece on the "
                               "stage first."));
  const bool is_drag = picked && picked->node.has_value();
  const QString why_not =
      !picked ? tr("Click a warp dot of a picked piece on the stage "
                   "first.")
              : tr("Tick \"Drags behind\" (or press D) first.");
  for (QDoubleSpinBox* field : AllFields()) {
    Explain(field, is_drag ? QString() : why_not);
  }
  Explain(&physics_, is_drag ? QString() : why_not);
  drags_.setChecked(is_drag);
  const bool is_physics = is_drag && picked->node->physics;
  physics_.setChecked(is_physics);
  for (QDoubleSpinBox* field : {&lag_, &bounce_}) {
    layout_.setRowVisible(field, !is_physics);
  }
  for (QDoubleSpinBox* field :
       {&stretch_, &inertia_, &air_, &gravity_, &spring_}) {
    layout_.setRowVisible(field, is_physics);
  }
  if (is_drag) {
    const DragNode& node = *picked->node;
    ShowNumber(&lag_, node.lag);
    ShowNumber(&bounce_, node.bounce);
    ShowNumber(&stretch_, node.stretch);
    ShowNumber(&inertia_, node.inertia);
    ShowNumber(&air_, node.air);
    ShowNumber(&gravity_, node.gravity);
    ShowNumber(&spring_, node.spring);
  }
}

void DragBox::Commit() {
  assert(managers_.IsComplete());
  const auto picked = Pick();
  const bool is_drag = picked && picked->node.has_value();
  if (!is_drag) {
    return;
  }
  DragNode node = *picked->node;
  node.lag = lag_.value();
  node.bounce = bounce_.value();
  node.stretch = stretch_.value();
  node.inertia = inertia_.value();
  node.air = air_.value();
  node.gravity = gravity_.value();
  node.spring = spring_.value();
  emit Problem(
      ProblemOf(managers_.rig->SetDrag(picked->doll, picked->piece, node)));
  assert(managers_.rig != nullptr);
}

}  // namespace snapper
