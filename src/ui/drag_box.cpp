#include "ui/drag_box.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>

#include "edit/rig_manager.h"
#include "ui/follow.h"
#include "ui/number_input.h"
#include "ui/form_helpers.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {
namespace {

double NumberOf(const DragNode& node, DragField field) {
  assert(static_cast<int>(field) >= 0);
  assert(node.point >= 0);
  switch (field) {
    case DragField::kLag:
      return node.lag;
    case DragField::kBounce:
      return node.bounce;
    case DragField::kStretch:
      return node.stretch;
    case DragField::kInertia:
      return node.inertia;
    case DragField::kAir:
      return node.air;
    case DragField::kGravity:
      return node.gravity;
    case DragField::kSpring:
      return node.spring;
  }
  return 0.0;
}

}  // namespace

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
  SetUpReachRow();
  layout_.addRow(QString(), &drags_);
  AddNumberRow(&layout_, tr("Lag"), &lag_, &live_);
  AddNumberRow(&layout_, tr("Bounce"), &bounce_, &live_);
  SetUpPhysicsRows();
  connect(&drags_, &QCheckBox::clicked, this, [this] {
    const Look look = LookAtPick();
    const bool is_all = !look.dots.empty() &&
                        look.nodes.size() == look.dots.size();
    emit Problem(ProblemOf(managers_.rig->SetDragNodes(look.dots, !is_all)));
    Refresh();
  });
  connect(&physics_, &QCheckBox::clicked, this, [this] {
    const Look look = LookAtPick();
    const bool is_all = !look.nodes.empty() &&
                        std::all_of(look.nodes.begin(), look.nodes.end(),
                                    [](const DragNode* n) {
                                      return n->physics;
                                    });
    emit Problem(ProblemOf(managers_.rig->SetPhysicsAll(look.dots, !is_all)));
    Refresh();
  });
  mirror_.setText(tr("Mirror to other side"));
  mirror_.setObjectName("mirror_dots");
  connect(&mirror_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(managers_.rig->MirrorDots(LookAtPick().dots)));
  });
  layout_.addRow(QString(), &mirror_);
  size_t index = 0;
  for (QDoubleSpinBox* field : AllFields()) {
    const DragField which = FieldAt(index++);
    MakeLiveNamed(
        field, &live_, this,
        [this, which] {
          return RigManager::FieldLabel(
              which, std::max<size_t>(LookAtPick().nodes.size(), 1));
        },
        [this, which, field] { Commit(which, field); });
  }
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 11);
}

void DragBox::SetUpReachRow() {
  assert(layout_.rowCount() == 0);
  assert(managers_.IsComplete());
  SetUpNumber(&reach_, Number::kCells);
  reach_.setObjectName("reach");
  reach_.setToolTip(tr("How far pulling the dot drags the dots around it, "
                       "like rubber: 0 moves it alone. The mouse wheel "
                       "over a picked dot changes it too."));
  AddNumberRow(&layout_, tr("Reach"), &reach_, &live_);
  MakeLiveNamed(
      &reach_, &live_, this,
      [this] {
        return RigManager::ReachLabel(
            std::max<size_t>(LookAtPick().dots.size(), 1));
      },
      [this] { CommitReach(); });
}

DragField DragBox::FieldAt(size_t index) {
  assert(index < 7);
  assert(kMaxWarpPoints > 0);
  constexpr DragField kOrder[] = {
      DragField::kLag,     DragField::kBounce,  DragField::kStretch,
      DragField::kInertia, DragField::kAir,     DragField::kGravity,
      DragField::kSpring};
  return kOrder[index];
}

std::array<QDoubleSpinBox*, 7> DragBox::AllFields() {
  assert(layout_.rowCount() >= 3);
  assert(managers_.IsComplete());
  return {&lag_, &bounce_, &stretch_, &inertia_, &air_, &gravity_, &spring_};
}

void DragBox::SetUpPhysicsRows() {
  assert(layout_.rowCount() == 4);
  assert(managers_.IsComplete());
  physics_.setText(tr("Physics"));
  physics_.setObjectName("physics");
  physics_.setToolTip(tr("Secondary motion: cloth, hair, a chest, soft "
                         "flesh. A lone physics dot jiggles and settles; "
                         "physics dots next to each other hang together "
                         "like cloth from the plain dots around them."));
  layout_.addRow(QString(), &physics_);
  const struct Row final {
    const char* name;
    QDoubleSpinBox* field;
    const char* label;
    const char* tip;
  } rows[] = {
      {"stretch", &stretch_, QT_TR_NOOP("Stretch"),
       QT_TR_NOOP("How far it may pull away from the dots around it: 0 "
                  "keeps its drawn distance (a lone dot cannot move), 1 "
                  "cell further.")},
      {"inertia", &inertia_, QT_TR_NOOP("Inertia"),
       QT_TR_NOOP("How much it keeps moving when the doll speeds up, "
                  "stops or turns: 0 moves with the doll, 1 is heavy and "
                  "swings through.")},
      {"air", &air_, QT_TR_NOOP("Air resistance"),
       QT_TR_NOOP("How fast its swinging dies down: 0 rings on longest, "
                  "1 settles soon.")},
      {"gravity", &gravity_, QT_TR_NOOP("Gravity"),
       QT_TR_NOOP("How hard it hangs down when the doll tilts or turns "
                  "over: 0 not at all, 1 strongly. Upright it stays as "
                  "drawn.")},
      {"spring", &spring_, QT_TR_NOOP("Bounce"),
       QT_TR_NOOP("How firmly it springs back to its drawn shape: 0 not "
                  "at all (cloth just hangs), 1 a firm jiggle.")}};
  for (const Row& row : rows) {
    SetUpNumber(row.field, Number::kFraction);
    row.field->setObjectName(row.name);
    row.field->setToolTip(tr(row.tip));
    AddNumberRow(&layout_, tr(row.label), row.field, &live_);
  }
}

DragBox::Look DragBox::LookAtPick() const {
  assert(managers_.selection != nullptr);
  assert(managers_.IsComplete());
  Look look;
  for (const PickedDot& dot : PickedDotsOf(managers_)) {
    look.dots.push_back({dot.doll, dot.piece, dot.point});
    look.reaches.push_back(ReachOf(*dot.rig, dot.point));
    const auto node = std::find_if(
        dot.rig->drag_nodes.begin(), dot.rig->drag_nodes.end(),
        [&dot](const DragNode& n) { return n.point == dot.point; });
    const bool is_drag = node != dot.rig->drag_nodes.end();
    if (is_drag) {
      look.nodes.push_back(&*node);
    }
  }
  return look;
}

void DragBox::Refresh() {
  assert(managers_.selection != nullptr);
  assert(managers_.IsComplete());
  const Look look = LookAtPick();
  const bool has_pick = !look.dots.empty();
  const bool has_node = !look.nodes.empty();
  const bool is_all_drag = has_node && look.nodes.size() == look.dots.size();
  const QString no_pick = tr("Click a warp dot of a picked piece on the "
                             "stage first.");
  Explain(&drags_, has_pick ? QString() : no_pick);
  Explain(&reach_, has_pick ? QString() : no_pick);
  if (has_pick) {
    ShowNumber(&reach_, look.reaches.front(), look.reaches);
  }
  const QString why_not =
      !has_pick ? no_pick : tr("Tick \"Drags behind\" (or press D) first.");
  for (QDoubleSpinBox* field : AllFields()) {
    Explain(field, has_node ? QString() : why_not);
  }
  Explain(&physics_, has_node ? QString() : why_not);
  Explain(&mirror_, has_pick ? WhyNoTwin(look.dots) : no_pick);
  drags_.setTristate(true);
  drags_.setCheckState(is_all_drag ? Qt::Checked
                       : has_node  ? Qt::PartiallyChecked
                                   : Qt::Unchecked);
  const auto n_physics = std::count_if(
      look.nodes.begin(), look.nodes.end(),
      [](const DragNode* n) { return n->physics; });
  const bool is_all_physics =
      has_node && static_cast<size_t>(n_physics) == look.nodes.size();
  physics_.setTristate(true);
  physics_.setCheckState(is_all_physics ? Qt::Checked
                         : n_physics > 0 ? Qt::PartiallyChecked
                                         : Qt::Unchecked);
  const bool has_plain = n_physics < static_cast<int>(look.nodes.size());
  for (QDoubleSpinBox* field : {&lag_, &bounce_}) {
    layout_.setRowVisible(field, has_plain || !has_node);
  }
  for (QDoubleSpinBox* field :
       {&stretch_, &inertia_, &air_, &gravity_, &spring_}) {
    layout_.setRowVisible(field, n_physics > 0);
  }
  if (!has_node) {
    return;
  }
  size_t index = 0;
  for (QDoubleSpinBox* field : AllFields()) {
    const DragField which = FieldAt(index++);
    std::vector<double> values;
    for (const DragNode* node : look.nodes) {
      const DragNode copy = *node;
      values.push_back(NumberOf(copy, which));
    }
    ShowNumber(field, values.front(), values);
  }
}

void DragBox::Commit(DragField field, const QDoubleSpinBox* box) {
  assert(managers_.IsComplete());
  assert(box != nullptr);
  const Look look = LookAtPick();
  const bool has_node = !look.nodes.empty();
  if (!has_node) {
    return;
  }
  // Typed math ("*2") works on each dot's own number.
  const QString math = box->property(kRelativeProperty).toString();
  const bool is_math = !math.isEmpty();
  emit Problem(ProblemOf(
      is_math ? managers_.rig->SetDragFieldMath(look.dots, field, math)
              : managers_.rig->SetDragField(look.dots, field,
                                            box->value())));
}

QString DragBox::WhyNoTwin(const std::vector<DotRef>& dots) const {
  assert(!dots.empty());
  assert(managers_.rig != nullptr);
  for (const DotRef& dot : dots) {
    const QString why = managers_.rig->WhyNoMirror(dot.doll, dot.piece);
    const bool is_refused = !why.isEmpty();
    if (is_refused) {
      return why;
    }
  }
  return QString();
}

void DragBox::CommitReach() {
  assert(managers_.IsComplete());
  assert(managers_.rig != nullptr);
  const Look look = LookAtPick();
  const bool has_pick = !look.dots.empty();
  if (!has_pick) {
    return;
  }
  // Typed math ("+1") works on each dot's own reach.
  const QString math = reach_.property(kRelativeProperty).toString();
  const bool is_math = !math.isEmpty();
  emit Problem(ProblemOf(
      is_math ? managers_.rig->SetWarpReachMath(look.dots, math)
              : managers_.rig->SetWarpReach(look.dots, reach_.value())));
}

}  // namespace snapper
