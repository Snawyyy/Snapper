#include "ui/drag_box.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {
DragBox::DragBox(const Managers& managers)
    : QGroupBox(tr("Warp dots")), managers_(managers), layout_(this),
      live_(managers.history), tiles_(managers.motion, this),
      presets_layout_(&presets_), pad_layout_(&pad_row_), pad_(this),
      blob_(managers.motion, this),
      fine_(tr("Fine tune"), managers.motion,
            managers.document->SettingsPath(), "drag_fine_tune", this) {
  assert(managers_.IsComplete());
  tiles_.setObjectName("kind_tiles");
  layout_.addRow(&tiles_);
  SetUpReachRow();
  SetUpPresets();
  SetUpPadRow();
  SetUpFineRows();
  mirror_.setText(tr("Mirror to other side"));
  mirror_.setObjectName("mirror_dots");
  layout_.addRow(&mirror_);
  connect(&mirror_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(managers_.rig->MirrorDots(LookAtPick().dots)));
  });
  connect(&tiles_, &KindTiles::Chosen, this,
          [this](Material material) { ChooseMaterial(material); });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 6);
}

void DragBox::SetUpReachRow() {
  assert(layout_.rowCount() == 1);
  assert(managers_.IsComplete());
  SetUpNumber(&reach_, Number::kCells);
  reach_.setButtonSymbols(QAbstractSpinBox::NoButtons);
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

void DragBox::SetUpPresets() {
  assert(layout_.rowCount() == 2);
  assert(chips_[0] == nullptr);
  presets_layout_.setContentsMargins(0, 0, 0, 0);
  for (size_t i = 0; i < kMaxPresetChips; ++i) {
    // Qt child of the chip row, which owns it.
    auto* chip = std::make_unique<QPushButton>(&presets_).release();
    chip->setObjectName(QStringLiteral("preset_%1").arg(i));
    chip->setCheckable(true);
    presets_layout_.addWidget(chip);
    chips_[i] = chip;
    connect(chip, &QPushButton::clicked, this,
            [this, i] { ChoosePreset(i); });
  }
  presets_layout_.addStretch(1);
  layout_.addRow(&presets_);
}

void DragBox::SetUpPadRow() {
  assert(layout_.rowCount() == 3);
  assert(managers_.IsComplete());
  pad_layout_.setContentsMargins(0, 0, 0, 0);
  pad_layout_.addWidget(&pad_);
  pad_layout_.addWidget(&blob_);
  pad_layout_.addStretch(1);
  blob_.setToolTip(tr("A live preview of how the picked dots will "
                      "wobble."));
  layout_.addRow(&pad_row_);
  connect(&pad_, &FeelPad::Grabbed, this, [this] {
    live_.Touch(RigManager::FeelLabel(
        std::max<size_t>(LookAtPick().dots.size(), 1)));
  });
  connect(&pad_, &FeelPad::Moved, this,
          [this](const Feel& feel) { MovePad(feel); });
  connect(&pad_, &FeelPad::Released, this, [this] { live_.Finish(); });
  connect(&pad_, &FeelPad::Cancelled, this, [this] {
    live_.Cancel();
    Refresh();
  });
}

void DragBox::SetUpFineRows() {
  assert(layout_.rowCount() == 4);
  assert(managers_.IsComplete());
  layout_.addRow(&fine_);
  const KindMix none((std::vector<const DragNode*>()));
  for (const DragField field : kFineFields) {
    QDoubleSpinBox* box = BoxOf(field);
    SetUpNumber(box, Number::kFraction);
    // Dragged by its name or typed into, as in the design: no arrows.
    box->setButtonSymbols(QAbstractSpinBox::NoButtons);
    AddNumberRow(fine_.form(), FineLabel(field, none), box, &live_);
    MakeLiveNamed(
        box, &live_, this,
        [this, field] {
          return RigManager::FieldLabel(
              field, std::max<size_t>(DotsUsing(LookAtPick(), field).size(),
                                      1));
        },
        [this, field, box] { CommitField(field, box); });
  }
  const char* const names[] = {"lag",   "bounce",  "stretch", "spring",
                               "inertia", "air", "gravity"};
  size_t index = 0;
  for (const DragField field : kFineFields) {
    BoxOf(field)->setObjectName(names[index++]);
  }
  fine_.setObjectName("fine_tune");
}

QDoubleSpinBox* DragBox::BoxOf(DragField field) {
  assert(static_cast<int>(field) >= 0);
  assert(managers_.IsComplete());
  switch (field) {
    case DragField::kLag:
      return &lag_;
    case DragField::kBounce:
      return &bounce_;
    case DragField::kStretch:
      return &stretch_;
    case DragField::kSpring:
      return &spring_;
    case DragField::kInertia:
      return &inertia_;
    case DragField::kAir:
      return &air_;
    case DragField::kGravity:
      return &gravity_;
  }
  return &lag_;
}

DragBox::Look DragBox::LookAtPick() const {
  assert(managers_.selection != nullptr);
  assert(managers_.IsComplete());
  Look look;
  for (const PickedDot& dot : PickedDotsOf(managers_)) {
    look.dots.push_back({dot.doll, dot.piece, dot.point});
    look.pieces.push_back(dot.piece);
    look.reaches.push_back(ReachOf(*dot.rig, dot.point));
    const auto node = std::find_if(
        dot.rig->drag_nodes.begin(), dot.rig->drag_nodes.end(),
        [&dot](const DragNode& n) { return n.point == dot.point; });
    const bool is_node = node != dot.rig->drag_nodes.end();
    look.per_dot.push_back(is_node ? &*node : nullptr);
    if (is_node) {
      look.nodes.push_back(&*node);
    }
  }
  return look;
}

void DragBox::Refresh() {
  assert(managers_.selection != nullptr);
  assert(managers_.IsComplete());
  const Look look = LookAtPick();
  const KindMix mix(look.per_dot);
  const bool has_pick = !look.dots.empty();
  const QString no_pick = tr("Click a warp dot of a picked piece on the "
                             "stage first.");
  setTitle(TitleOf(look.pieces, mix));
  tiles_.Show(mix, has_pick ? QString() : no_pick);
  Explain(&reach_, has_pick ? QString() : no_pick);
  Explain(&mirror_, has_pick ? WhyNoTwin(look.dots) : no_pick);
  if (has_pick) {
    ShowNumber(&reach_, look.reaches.front(), look.reaches);
  }
  ShowPresets(look, mix);
  ShowPad(look, no_pick);
  ShowFine(look, mix, no_pick);
}

void DragBox::ChooseMaterial(Material material) {
  assert(managers_.rig != nullptr);
  assert(static_cast<int>(material) < kMaterialCount);
  const Look look = LookAtPick();
  const bool has_pick = !look.dots.empty();
  if (!has_pick) {
    return;
  }
  const bool is_still = material == Material::kStill;
  emit Problem(ProblemOf(
      is_still ? managers_.rig->SetDragNodes(look.dots, false)
               : managers_.rig->SetNodeKindAll(look.dots, KindOf(material))));
}

}  // namespace snapper
