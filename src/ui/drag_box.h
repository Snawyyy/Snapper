#ifndef SNAPPER_UI_DRAG_BOX_H_
#define SNAPPER_UI_DRAG_BOX_H_

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include <array>
#include <vector>

#include "anim/node_feel.h"
#include "edit/rig_manager.h"
#include "model/doll.h"
#include "ui/dot_kinds.h"
#include "ui/feel_pad.h"
#include "ui/fold_section.h"
#include "ui/live_edit.h"
#include "ui/kind_tiles.h"
#include "ui/managers.h"
#include "ui/wobble_blob.h"

namespace snapper {

// The exact rows, in the order they show.
inline constexpr DragField kFineFields[] = {
    DragField::kLag,     DragField::kBounce,  DragField::kStretch,
    DragField::kSpring,  DragField::kInertia, DragField::kAir,
    DragField::kGravity};

// The most preset chips a kind may have.
inline constexpr size_t kMaxPresetChips = 4;

// The warp dots picked on the stage, as a game-like panel: four
// material tiles (Still, Drag, Cloth, Jelly) that set what the picked
// dots are made of, the rubber reach, preset chips and a feel pad with
// a live wobble beside it for how they move, a folded section with the
// exact numbers, and a Mirror button. Every picked dot is edited at
// once: a number that differs shows Mixed, and each change is one undo
// step. Greyed out with the reason when nothing can act.
class DragBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DragBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The picked dots; per dot its drag node, null when it is Still.
  struct Look final {
    std::vector<DotRef> dots;
    std::vector<QString> pieces;
    std::vector<double> reaches;
    std::vector<const DragNode*> per_dot;
    std::vector<const DragNode*> nodes;
  };
  Look LookAtPick() const;
  // The picked dots whose node uses the row's number.
  std::vector<DotRef> DotsUsing(const Look& look, DragField field) const;
  // The three changes the tiles, chips and pad make.
  void ChooseMaterial(Material material);
  void ChoosePreset(size_t index);
  void MovePad(const Feel& feel);
  void CommitField(DragField field, const QDoubleSpinBox* box);
  void CommitReach();
  // Why the picked dots can't be mirrored; empty when they can.
  QString WhyNoTwin(const std::vector<DotRef>& dots) const;

  void SetUpReachRow();
  void SetUpPadRow();
  void SetUpPresets();
  void SetUpFineRows();
  void ShowPresets(const Look& look, const KindMix& mix);
  void ShowPad(const Look& look, const QString& no_pick);
  void ShowFine(const Look& look, const KindMix& mix,
                const QString& no_pick);
  QDoubleSpinBox* BoxOf(DragField field);

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  KindTiles tiles_;
  QDoubleSpinBox reach_;
  QWidget presets_;
  QHBoxLayout presets_layout_;
  std::array<QPushButton*, kMaxPresetChips> chips_{};
  std::vector<FeelPreset> shown_presets_;
  QWidget pad_row_;
  QHBoxLayout pad_layout_;
  FeelPad pad_;
  WobbleBlob blob_;
  FoldSection fine_;
  QPushButton mirror_;
  QDoubleSpinBox lag_;
  QDoubleSpinBox bounce_;
  QDoubleSpinBox stretch_;
  QDoubleSpinBox spring_;
  QDoubleSpinBox inertia_;
  QDoubleSpinBox air_;
  QDoubleSpinBox gravity_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_BOX_H_
