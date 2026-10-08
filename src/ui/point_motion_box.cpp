#include "ui/point_motion_box.h"

#include <QSignalBlocker>

#include <cassert>

#include "edit/rig_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {
namespace {

QString KindLabel(WarpMotionKind kind) {
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kWarpMotionKindCount);
  switch (kind) {
    case WarpMotionKind::kNone:
      return PointMotionBox::tr("None");
    case WarpMotionKind::kWave:
      return PointMotionBox::tr("Wave");
    case WarpMotionKind::kPulse:
      return PointMotionBox::tr("Pulse");
    case WarpMotionKind::kBreathe:
      return PointMotionBox::tr("Breathe");
    case WarpMotionKind::kSway:
      return PointMotionBox::tr("Sway");
    case WarpMotionKind::kShiver:
      return PointMotionBox::tr("Shiver");
  }
  assert(false);
  return QString();
}

// The picked dot's motion, or a fresh one on it when it has none.
PointMotion MotionOf(const PickedDot& dot) {
  assert(dot.rig != nullptr);
  assert(dot.point >= 0);
  const PointMotion* found = FindMotion(*dot.rig, dot.point);
  PointMotion fresh;
  fresh.point = dot.point;
  return found != nullptr ? *found : fresh;
}

}  // namespace

PointMotionBox::PointMotionBox(const Managers& managers)
    : QGroupBox(tr("Dot animation")), managers_(managers), layout_(this),
      live_(managers.history) {
  assert(managers_.IsComplete());
  Build();
  connect(&kind_, &QComboBox::activated, this, [this](int index) {
    SetKind(static_cast<WarpMotionKind>(index));
  });
  for (QDoubleSpinBox* field : {&size_, &angle_, &delay_}) {
    MakeLive(field, &live_, tr("Point animation"), this,
             [this] { Commit(); });
  }
  MakeLive(&cycle_, &live_, tr("Point animation"), this,
           [this] { Commit(); });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 5);
}

void PointMotionBox::Build() {
  assert(kind_.count() == 0);
  assert(layout_.rowCount() == 0);
  for (int kind = 0; kind < kWarpMotionKindCount; ++kind) {
    kind_.addItem(KindLabel(static_cast<WarpMotionKind>(kind)));
  }
  kind_.setObjectName("point_motion");
  size_.setObjectName("motion_size");
  cycle_.setObjectName("motion_cycle");
  angle_.setObjectName("motion_angle");
  delay_.setObjectName("motion_delay");
  SetUpNumber(&size_, Number::kSize);
  size_.setMaximum(kMaxWarpMotionSize);
  cycle_.setRange(kMinWarpCycle, kMaxWarpCycle);
  cycle_.setPrefix(tr("every "));
  cycle_.setSuffix(tr(" fr"));
  SetUpNumber(&angle_, Number::kDegrees);
  angle_.setRange(-180.0, 180.0);
  angle_.setWrapping(true);
  SetUpNumber(&delay_, Number::kFraction);
  layout_.addRow(tr("Moves"), &kind_);
  layout_.addRow(tr("Size"), &size_);
  layout_.addRow(tr("Cycle"), &cycle_);
  layout_.addRow(tr("Direction"), &angle_);
  layout_.addRow(tr("Delay"), &delay_);
}

void PointMotionBox::SetKind(WarpMotionKind kind) {
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kWarpMotionKindCount);
  assert(managers_.rig != nullptr);
  const auto dot = PickedDotOf(managers_);
  if (!dot) {
    return;
  }
  PointMotion motion = MotionOf(*dot);
  motion.kind = kind;
  emit Problem(ProblemOf(
      managers_.rig->SetPointMotion(dot->doll, dot->piece, motion)));
}

void PointMotionBox::Commit() {
  assert(managers_.rig != nullptr);
  const auto dot = PickedDotOf(managers_);
  const bool is_moving =
      dot && FindMotion(*dot->rig, dot->point) != nullptr;
  if (!is_moving) {
    return;
  }
  PointMotion motion = MotionOf(*dot);
  motion.size = size_.value();
  motion.cycle = cycle_.value();
  motion.angle = angle_.value();
  motion.delay = delay_.value();
  emit Problem(ProblemOf(
      managers_.rig->SetPointMotion(dot->doll, dot->piece, motion)));
  assert(motion.point == dot->point);
}

void PointMotionBox::Refresh() {
  assert(kind_.count() == kWarpMotionKindCount);
  assert(managers_.selection != nullptr);
  const auto dot = PickedDotOf(managers_);
  const PointMotion* found =
      dot ? FindMotion(*dot->rig, dot->point) : nullptr;
  const QString no_dot =
      dot ? QString()
          : tr("Click a warp dot of a picked piece on the stage first.");
  const QString why_not =
      !dot ? no_dot : found != nullptr ? QString()
                                       : tr("Pick how the dot moves first.");
  const bool is_shiver =
      found != nullptr && found->kind == WarpMotionKind::kShiver;
  Explain(&kind_, no_dot,
          tr("Moves the dot by itself, on top of its keys: hair that "
             "waves, a heart that beats. Its rubber reach takes the "
             "dots around along."));
  Explain(&size_, why_not, tr("How far the dot moves at most."));
  Explain(&cycle_, why_not, tr("Frames one cycle takes; 24 is a second."));
  Explain(&angle_,
          is_shiver ? tr("A shiver shakes every way, not one.") : why_not,
          tr("The way the dot moves: 0 is right, 90 is down."));
  Explain(&delay_, why_not,
          tr("How late in its cycle the dot starts, 0 to 1. Give dots "
             "in a row rising delays for a ripple."));
  const PointMotion shown = found != nullptr ? *found : PointMotion();
  const QSignalBlocker quiet(kind_);
  kind_.setCurrentIndex(found != nullptr ? static_cast<int>(shown.kind) : 0);
  ShowNumber(&size_, shown.size);
  ShowNumber(&cycle_, shown.cycle);
  ShowNumber(&angle_, shown.angle);
  ShowNumber(&delay_, shown.delay);
}

}  // namespace snapper
