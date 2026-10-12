#include "ui/drag_box.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/pose_controls.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// The top row's width shares: the pad takes the golden ratio's long
// part, its preview the short one (62 to 38).
constexpr int kGoldenLong = 62;
constexpr int kGoldenShort = 38;

}  // namespace

DragBox::DragBox(const Managers& managers)
    : QGroupBox(tr("Warp dots")), managers_(managers), outer_(this),
      layout_(&dots_part_), live_(managers.history),
      tiles_(managers.motion, this), presets_layout_(&presets_),
      pad_(this), blob_(managers.motion, this),
      fine_(tr("Fine tune"), managers.motion,
            managers.document->SettingsPath(), "drag_fine_tune", this),
      groups_(managers) {
  assert(managers_.IsComplete());
  tiles_.setObjectName("kind_tiles");
  layout_.addRow(&tiles_);
  SetUpPadRow();
  SetUpPresets();
  SetUpReachRow();
  SetUpFineRows();
  mirror_.setText(tr("Mirror to other side"));
  mirror_.setObjectName("mirror_dots");
  default_.setText(tr("Set as default"));
  default_.setObjectName("set_default");
  layout_.addRow(MakeCellRow({&mirror_, &default_}, {}, this));
  connect(&mirror_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(managers_.rig->MirrorDots(LookAtPick().dots)));
  });
  connect(&default_, &QPushButton::clicked, this, [this] {
    const std::vector<DotRef> dots = LookAtPick().dots;
    const bool has_dot = !dots.empty();
    if (has_dot) {
      emit Problem(ProblemOf(
          managers_.rig->SetDotsDefault(dots.front().doll,
                                        dots.front().layer)));
    }
  });
  connect(&tiles_, &KindTiles::Chosen, this,
          [this](Material material) { ChooseMaterial(material); });
  // The groups: a flat group box inside a plate and a sunken frame.
  groups_.setTitle(QString());
  groups_.setFlat(true);
  groups_cell_ = MakePlated(tr("Groups"), &groups_, this);
  connect(&groups_, &GroupBox::Problem, this, &DragBox::Problem);
  outer_.setContentsMargins(4, 4, 4, 4);
  outer_.setSpacing(8);
  outer_.addWidget(&dots_part_);
  outer_.addWidget(groups_cell_);
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 6);
}

void DragBox::ShowParts(bool has_dots, bool has_groups) {
  assert(groups_cell_ != nullptr);
  assert(managers_.IsComplete());
  dots_part_.setVisible(has_dots);
  groups_cell_->setVisible(has_groups);
  setVisible(has_dots || has_groups);
}

void DragBox::SetUpReachRow() {
  assert(layout_.rowCount() == 3);
  assert(managers_.IsComplete());
  SetUpNumber(&reach_, Number::kCells);
  reach_.setButtonSymbols(QAbstractSpinBox::NoButtons);
  reach_.setObjectName("reach");
  reach_.setToolTip(tr("How far pulling the dot drags the dots around it, "
                       "like rubber: 0 moves it alone. The mouse wheel "
                       "over a picked dot changes it too."));
  layout_.addRow(MakeFaderRow(tr("Reach"), &reach_, &live_,
                              static_cast<double>(kMaxWarpCells), 0.0,
                              this));
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
  assert(layout_.rowCount() == 1);
  assert(managers_.IsComplete());
  blob_.setToolTip(tr("A live preview of how the picked dots will "
                      "wobble."));
  // The pad and its preview, the pad taking the golden ratio's long part.
  blob_.setFixedHeight(pad_.sizeHint().height());
  layout_.addRow(MakeCellRow({MakePlated(tr("Feel"), &pad_, this),
                              MakePlated(tr("Preview"), &blob_, this)},
                             {kGoldenLong, kGoldenShort}, this));
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
    // The fader returns to the value a new node starts with.
    fine_.form()->addRow(MakeFaderRow(FineLabel(field, none), box, &live_,
                                      1.0, FieldValue(DragNode(), field),
                                      this));
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
    look.dots.push_back({dot.doll, dot.piece, dot.point, dot.layer});
    look.keep.push_back(dot.rig);
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
  // Set as default needs settings this layer has changed for itself.
  const bool is_own = has_pick && managers_.rig->HasOwnDots(
                                      look.dots.front().doll,
                                      look.dots.front().layer);
  Explain(&default_, !has_pick ? no_pick
                     : !is_own ? tr("These settings already are the doll's "
                                    "default.")
                               : QString(),
          tr("Make these warp dot settings the doll's default: every copy "
             "of the doll added later starts with them. Shots that "
             "already have the doll keep what they have."));
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
