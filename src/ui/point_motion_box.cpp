#include "ui/point_motion_box.h"

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

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

// The picked dots, and the motions the ones that move have.
struct Look final {
  std::vector<DotRef> dots;
  std::vector<DotRef> moving;
  std::vector<const PointMotion*> motions;
};

Look LookAt(const Managers& managers) {
  assert(managers.selection != nullptr);
  assert(managers.history != nullptr);
  Look look;
  for (const PickedDot& dot : PickedDotsOf(managers)) {
    look.dots.push_back({dot.doll, dot.piece, dot.point});
    const PointMotion* found = FindMotion(*dot.rig, dot.point);
    const bool is_moving = found != nullptr;
    if (is_moving) {
      look.moving.push_back(look.dots.back());
      look.motions.push_back(found);
    }
  }
  return look;
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
  const auto label = [this](MotionField field) {
    return RigManager::MotionLabel(
        field, std::max<size_t>(LookAt(managers_).moving.size(), 1));
  };
  MakeLiveNamed(&size_, &live_, this,
                [label] { return label(MotionField::kSize); },
                [this] { Commit(MotionField::kSize, size_.value()); });
  MakeLiveNamed(&angle_, &live_, this,
                [label] { return label(MotionField::kAngle); },
                [this] { Commit(MotionField::kAngle, angle_.value()); });
  MakeLiveNamed(&delay_, &live_, this,
                [label] { return label(MotionField::kDelay); },
                [this] { Commit(MotionField::kDelay, delay_.value()); });
  MakeLiveNamed(&cycle_, &live_, this,
                [label] { return label(MotionField::kCycle); },
                [this] { Commit(MotionField::kCycle, cycle_.value()); });
  kind_.setPlaceholderText(tr("Mixed"));
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
  AddNumberRow(&layout_, tr("Size"), &size_, &live_);
  AddNumberRow(&layout_, tr("Cycle"), &cycle_, &live_);
  AddNumberRow(&layout_, tr("Direction"), &angle_, &live_);
  AddNumberRow(&layout_, tr("Delay"), &delay_, &live_);
}

void PointMotionBox::SetKind(WarpMotionKind kind) {
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kWarpMotionKindCount);
  assert(managers_.rig != nullptr);
  const Look look = LookAt(managers_);
  const bool has_dots = !look.dots.empty();
  if (!has_dots) {
    return;
  }
  emit Problem(ProblemOf(managers_.rig->SetMotionAll(
      look.dots, MotionField::kKind, static_cast<double>(kind))));
}

void PointMotionBox::Commit(MotionField field, double value) {
  assert(managers_.rig != nullptr);
  assert(std::isfinite(value));
  const Look look = LookAt(managers_);
  const bool is_moving = !look.moving.empty();
  if (!is_moving) {
    return;
  }
  emit Problem(
      ProblemOf(managers_.rig->SetMotionAll(look.moving, field, value)));
}

void PointMotionBox::Refresh() {
  assert(kind_.count() == kWarpMotionKindCount);
  assert(managers_.selection != nullptr);
  const Look look = LookAt(managers_);
  const bool has_pick = !look.dots.empty();
  const bool has_motion = !look.motions.empty();
  const QString no_dot =
      has_pick ? QString()
               : tr("Click a warp dot of a picked piece on the stage first.");
  const QString why_not =
      !has_pick ? no_dot : has_motion ? QString()
                                      : tr("Pick how the dot moves first.");
  const bool is_shiver =
      has_motion &&
      std::all_of(look.motions.begin(), look.motions.end(),
                  [](const PointMotion* m) {
                    return m->kind == WarpMotionKind::kShiver;
                  });
  Explain(&kind_, no_dot,
          tr("Moves the dots by themselves, on top of their keys: hair "
             "that waves, a heart that beats. Their rubber reach takes "
             "the dots around along."));
  Explain(&size_, why_not, tr("How far the dot moves at most."));
  Explain(&cycle_, why_not, tr("Frames one cycle takes; 24 is a second."));
  Explain(&angle_,
          is_shiver ? tr("A shiver shakes every way, not one.") : why_not,
          tr("The way the dot moves: 0 is right, 90 is down."));
  Explain(&delay_, why_not,
          tr("How late in its cycle the dot starts, 0 to 1. Give dots "
             "in a row rising delays for a ripple."));
  const QSignalBlocker quiet(kind_);
  const bool is_one_kind =
      has_motion && look.motions.size() == look.dots.size() &&
      std::all_of(look.motions.begin(), look.motions.end(),
                  [&look](const PointMotion* m) {
                    return m->kind == look.motions.front()->kind;
                  });
  kind_.setCurrentIndex(is_one_kind
                            ? static_cast<int>(look.motions.front()->kind)
                            : has_motion ? -1 : 0);
  if (!has_motion) {
    return;
  }
  const auto numbers = [&look](double PointMotion::*field) {
    std::vector<double> values;
    for (const PointMotion* m : look.motions) {
      values.push_back(m->*field);
    }
    return values;
  };
  const PointMotion& first = *look.motions.front();
  ShowNumber(&size_, first.size, numbers(&PointMotion::size));
  ShowNumber(&angle_, first.angle, numbers(&PointMotion::angle));
  ShowNumber(&delay_, first.delay, numbers(&PointMotion::delay));
  std::vector<double> cycles;
  for (const PointMotion* m : look.motions) {
    cycles.push_back(m->cycle);
  }
  ShowNumber(&cycle_, first.cycle, cycles);
}

}  // namespace snapper
