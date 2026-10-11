// What the warp dot panel shows and commits, apart from its set-up.
#include <QLabel>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "ui/drag_box.h"
#include "ui/form_helpers.h"
#include "ui/number_input.h"
#include "ui/problem.h"

namespace snapper {
std::vector<DotRef> DragBox::DotsUsing(const Look& look,
                                       DragField field) const {
  assert(look.dots.size() == look.per_dot.size());
  assert(static_cast<int>(field) >= 0);
  std::vector<DotRef> dots;
  for (size_t i = 0; i < look.dots.size(); ++i) {
    const DragNode* node = look.per_dot[i];
    const bool is_user =
        node != nullptr && RowAppliesTo(field, MaterialOf(node));
    if (is_user) {
      dots.push_back(look.dots[i]);
    }
  }
  return dots;
}

void DragBox::ShowPresets(const Look& look, const KindMix& mix) {
  assert(look.dots.size() == look.per_dot.size());
  assert(chips_[0] != nullptr);
  const bool is_one_kind = mix.total() > 0 && !mix.IsMixed() &&
                           mix.Chosen() != Material::kStill;
  presets_.setVisible(is_one_kind);
  shown_presets_.clear();
  if (!is_one_kind) {
    return;
  }
  shown_presets_ = FeelPresets(KindOf(mix.Chosen()));
  assert(shown_presets_.size() <= kMaxPresetChips);
  const std::vector<Feel> feels = DistinctFeels(look.nodes);
  const bool is_same = feels.size() == 1;
  for (size_t i = 0; i < kMaxPresetChips; ++i) {
    const bool has_preset = i < shown_presets_.size();
    chips_[i]->setVisible(has_preset);
    if (has_preset) {
      const FeelPreset& preset = shown_presets_[i];
      chips_[i]->setText(preset.name);
      chips_[i]->setToolTip(tr("Set the picked dots to %1.").arg(preset.name));
      chips_[i]->setChecked(is_same && SameFeel(feels.front(), preset.feel));
    }
  }
}

void DragBox::ShowPad(const Look& look, const QString& no_pick) {
  assert(look.dots.size() == look.per_dot.size());
  assert(!no_pick.isEmpty());
  const bool has_node = !look.nodes.empty();
  const bool has_pick = !look.dots.empty();
  const QString why =
      !has_pick ? no_pick
      : !has_node ? tr("Make the dots Drag, Cloth or Jelly first.")
                  : QString();
  Explain(&pad_, why,
          tr("Drag the puck: left is soft, right firm, up heavy, down "
             "light. Escape while dragging puts it back."));
  Explain(&blob_, why, tr("A live preview of how the picked dots wobble."));
  pad_.SetFeels(DistinctFeels(look.nodes));
  // Mixed picks show the first picked dot.
  blob_.SetNode(has_node ? look.nodes.front() : nullptr);
}

void DragBox::ShowFine(const Look& look, const KindMix& mix,
                       const QString& no_pick) {
  assert(look.dots.size() == look.per_dot.size());
  assert(!no_pick.isEmpty());
  const bool has_node = !look.nodes.empty();
  const QString why = look.dots.empty()
                          ? no_pick
                          : tr("Make the dots Drag, Cloth or Jelly first.");
  Explain(&fine_, has_node ? QString() : why);
  const std::vector<DragField> rows = FineRowsOf(mix);
  for (const DragField field : kFineFields) {
    QDoubleSpinBox* box = BoxOf(field);
    const bool is_row =
        std::find(rows.begin(), rows.end(), field) != rows.end();
    fine_.form()->setRowVisible(box, is_row);
    Explain(box, has_node ? QString() : why, FineHelp(field, mix));
    auto* label = qobject_cast<QLabel*>(fine_.form()->labelForField(box));
    const bool has_label = label != nullptr;
    if (has_label) {
      label->setText(FineLabel(field, mix));
    }
    std::vector<double> values;
    for (const DragNode* node : look.nodes) {
      const bool is_user = RowAppliesTo(field, MaterialOf(node));
      if (is_user) {
        values.push_back(FieldValue(*node, field));
      }
    }
    const bool has_values = is_row && !values.empty();
    if (has_values) {
      ShowNumber(box, values.front(), values);
    }
  }
}

void DragBox::MovePad(const Feel& feel) {
  assert(managers_.rig != nullptr);
  assert(std::isfinite(feel.firm + feel.heavy));
  const Look look = LookAtPick();
  const bool has_node = !look.nodes.empty();
  if (has_node) {
    emit Problem(ProblemOf(managers_.rig->SetFeelAll(look.dots, feel)));
  }
}

void DragBox::ChoosePreset(size_t index) {
  assert(managers_.rig != nullptr);
  assert(shown_presets_.size() <= kMaxPresetChips);
  const bool is_known = index < shown_presets_.size();
  if (is_known) {
    const Look look = LookAtPick();
    emit Problem(ProblemOf(managers_.rig->SetFeelAll(
        look.dots, shown_presets_[index].feel)));
  }
  // A click on a lit chip must not leave it dark.
  Refresh();
}

void DragBox::CommitField(DragField field, const QDoubleSpinBox* box) {
  assert(managers_.IsComplete());
  assert(box != nullptr);
  const std::vector<DotRef> users = DotsUsing(LookAtPick(), field);
  const bool has_user = !users.empty();
  if (!has_user) {
    return;
  }
  // Typed math ("*2") works on each dot's own number.
  const QString math = box->property(kRelativeProperty).toString();
  const bool is_math = !math.isEmpty();
  emit Problem(ProblemOf(
      is_math ? managers_.rig->SetDragFieldMath(users, field, math)
              : managers_.rig->SetDragField(users, field, box->value())));
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
