// The rig panel's Animation rows: how a piece's warp grid moves by
// itself.

#include <cassert>

#include "edit/rig_manager.h"
#include "ui/form_helpers.h"
#include "ui/live_edit.h"
#include "ui/problem.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

QString MotionLabel(WarpMotionKind kind) {
  switch (kind) {
    case WarpMotionKind::kNone:
      return RigPanel::tr("None");
  }
  assert(false);
  return QString();
}

QString EdgeLabel(WarpEdge edge) {
  switch (edge) {
    case WarpEdge::kTop:
      return RigPanel::tr("Top");
    case WarpEdge::kLeft:
      return RigPanel::tr("Left");
    case WarpEdge::kBottom:
      return RigPanel::tr("Bottom");
    case WarpEdge::kRight:
      return RigPanel::tr("Right");
  }
  assert(false);
  return QString();
}

}  // namespace

void RigPanel::BuildMotion() {
  assert(motion_.count() == 0);
  assert(motion_edge_.count() == 0);
  for (int kind = 0; kind < kWarpMotionKindCount; ++kind) {
    motion_.addItem(MotionLabel(static_cast<WarpMotionKind>(kind)));
  }
  for (int edge = 0; edge < kWarpEdgeCount; ++edge) {
    motion_edge_.addItem(EdgeLabel(static_cast<WarpEdge>(edge)));
  }
  motion_.setObjectName("warp_motion");
  motion_.setToolTip(tr("Moves the grid by itself, on top of its keys: "
                        "hair that waves, a heart that beats."));
  motion_edge_.setToolTip(tr("The edge that stays put; the far side "
                             "moves most."));
  SetUpNumber(&motion_size_, Number::kSize, tr("size "));
  motion_size_.setMaximum(kMaxWarpMotionSize);
  motion_size_.setToolTip(tr("How far the dots move at most."));
  motion_cycle_.setRange(kMinWarpCycle, kMaxWarpCycle);
  motion_cycle_.setPrefix(tr("every "));
  motion_cycle_.setSuffix(tr(" fr"));
  motion_cycle_.setToolTip(tr("Frames one cycle takes; 24 is a second."));
  form_.addRow(tr("Animation"), &motion_);
  form_.addRow(tr("Anchor"), &motion_edge_);
  AddPair(&form_, tr("Amount"), &motion_row_, &motion_size_,
          &motion_cycle_);
  assert(motion_.count() == kWarpMotionKindCount);
}

void RigPanel::WireMotion() {
  RigManager* rig = managers_.rig;
  assert(rig != nullptr);
  assert(motion_.count() == kWarpMotionKindCount);
  connect(&motion_, &QComboBox::activated, this, [this, rig](int index) {
    emit Problem(ProblemOf(rig->SetMotionKindAll(
        doll_, Picked(), static_cast<WarpMotionKind>(index))));
  });
  connect(&motion_edge_, &QComboBox::activated, this, [this, rig](int index) {
    emit Problem(ProblemOf(rig->SetMotionEdgeAll(
        doll_, Picked(), static_cast<WarpEdge>(index))));
  });
  MakeLive(&motion_size_, &live_, tr("Animation size"), this, [this, rig] {
    const RigPiece* focus = Focus();
    const bool has_focus = focus != nullptr;
    if (has_focus) {
      emit Problem(ProblemOf(rig->ShiftMotionAll(
          doll_, Picked(), motion_size_.value() - focus->warp_motion.size,
          0)));
    }
  });
  MakeLive(&motion_cycle_, &live_, tr("Animation speed"), this, [this, rig] {
    const RigPiece* focus = Focus();
    const bool has_focus = focus != nullptr;
    if (has_focus) {
      emit Problem(ProblemOf(rig->ShiftMotionAll(
          doll_, Picked(), 0.0,
          motion_cycle_.value() - focus->warp_motion.cycle)));
    }
  });
}

void RigPanel::RefreshMotion(const RigPiece* rig) {
  const bool has_grid = rig != nullptr && rig->warp.IsOn();
  const QString no_grid = rig == nullptr ? tr("Pick a piece first.")
                          : has_grid     ? QString()
                                     : tr("Tick \"Bend with a grid\" first.");
  Explain(&motion_, no_grid);
  if (!has_grid) {
    for (QWidget* field : {static_cast<QWidget*>(&motion_edge_),
                           static_cast<QWidget*>(&motion_size_),
                           static_cast<QWidget*>(&motion_cycle_)}) {
      Explain(field, no_grid);
    }
    return;
  }
  const WarpMotion& motion = rig->warp_motion;
  const QString no_motion =
      motion.IsOn() ? QString() : tr("Pick an animation first.");
  for (QWidget* field : {static_cast<QWidget*>(&motion_edge_),
                         static_cast<QWidget*>(&motion_size_),
                         static_cast<QWidget*>(&motion_cycle_)}) {
    Explain(field, no_motion);
  }
  const QSignalBlocker quiet_kind(motion_);
  const QSignalBlocker quiet_edge(motion_edge_);
  motion_.setCurrentIndex(static_cast<int>(motion.kind));
  motion_edge_.setCurrentIndex(static_cast<int>(motion.edge));
  ShowNumber(&motion_size_, motion.size);
  ShowNumber(&motion_cycle_, motion.cycle);
}

}  // namespace snapper
