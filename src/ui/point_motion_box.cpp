#include "ui/point_motion_box.h"

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "edit/rig_manager.h"
#include "ui/follow.h"
#include "ui/number_input.h"
#include "ui/form_helpers.h"
#include "ui/picked_dot.h"
#include "ui/problem.h"

namespace snapper {
namespace {

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

// Outlines the tiles the picked dots use (a dot with no motion is
// None): thick when every dot moves that way, thin when some do.
void ShowTiles(const std::array<MotionTile*, kWarpMotionKindCount>& tiles,
               const Look& look, const QString& no_dot) {
  assert(tiles[0] != nullptr);
  assert(look.motions.size() <= look.dots.size());
  std::array<size_t, kWarpMotionKindCount> counts{};
  counts[static_cast<size_t>(WarpMotionKind::kNone)] =
      look.dots.size() - look.motions.size();
  for (const PointMotion* m : look.motions) {
    ++counts[static_cast<size_t>(m->kind)];
  }
  for (MotionTile* tile : tiles) {
    const size_t used = counts[static_cast<size_t>(tile->kind())];
    const bool is_all = used > 0 && used == look.dots.size();
    tile->SetShare(is_all ? Share::kAll
                          : used > 0 ? Share::kSome : Share::kNone);
    Explain(tile, no_dot,
            PointMotionBox::tr(
                "Moves the dots by themselves, on top of their keys: hair "
                "that waves, a heart that beats. Their rubber reach takes "
                "the dots around along."));
  }
}

}  // namespace

PointMotionBox::PointMotionBox(const Managers& managers)
    : QGroupBox(tr("Dot animation")), managers_(managers), layout_(this),
      live_(managers.history), kinds_layout_(&kinds_) {
  assert(managers_.IsComplete());
  Build();
  const auto label = [this](MotionField field) {
    return RigManager::MotionLabel(
        field, std::max<size_t>(LookAt(managers_).moving.size(), 1));
  };
  MakeLiveNamed(&size_, &live_, this,
                [label] { return label(MotionField::kSize); },
                [this] { Commit(MotionField::kSize, &size_); });
  MakeLiveNamed(&angle_, &live_, this,
                [label] { return label(MotionField::kAngle); },
                [this] { Commit(MotionField::kAngle, &angle_); });
  MakeLiveNamed(&delay_, &live_, this,
                [label] { return label(MotionField::kDelay); },
                [this] { Commit(MotionField::kDelay, &delay_); });
  MakeLiveNamed(&cycle_, &live_, this,
                [label] { return label(MotionField::kCycle); },
                [this] { Commit(MotionField::kCycle, &cycle_); });
  Follow(managers_, this);
  Refresh();
  assert(layout_.rowCount() == 5);
}

void PointMotionBox::Build() {
  assert(tiles_[0] == nullptr);
  assert(layout_.rowCount() == 0);
  constexpr int kPerRow = 3;
  kinds_layout_.setContentsMargins(0, 0, 0, 0);
  for (int i = 0; i < kWarpMotionKindCount; ++i) {
    const auto kind = static_cast<WarpMotionKind>(i);
    // A Qt child of the tile row, which owns it.
    MotionTile* tile =
        std::make_unique<MotionTile>(kind, managers_.motion, &kinds_)
            .release();
    tile->setObjectName(QStringLiteral("motion_kind_%1").arg(i));
    kinds_layout_.addWidget(tile, i / kPerRow, i % kPerRow);
    tiles_[static_cast<size_t>(i)] = tile;
    connect(tile, &QAbstractButton::clicked, this,
            [this, kind] { SetKind(kind); });
  }
  kinds_.setObjectName("point_motion");
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
  layout_.addRow(tr("Moves"), &kinds_);
  AddNumberRow(&layout_, tr("Size"), &size_, &live_);
  AddNumberRow(&layout_, tr("Cycle"), &cycle_, &live_);
  AddAngleRow(&layout_, tr("Direction"), &angle_, &live_);
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

void PointMotionBox::Commit(MotionField field,
                            const QAbstractSpinBox* box) {
  assert(managers_.rig != nullptr);
  assert(box != nullptr);
  const Look look = LookAt(managers_);
  const bool is_moving = !look.moving.empty();
  if (!is_moving) {
    return;
  }
  // Typed math ("*2") works on each dot's own number.
  const QString math = box->property(kRelativeProperty).toString();
  const bool is_math = !math.isEmpty();
  emit Problem(ProblemOf(
      is_math ? managers_.rig->SetMotionMath(look.moving, field, math)
              : managers_.rig->SetMotionAll(
                    look.moving, field,
                    box->property("value").toDouble())));
}

void PointMotionBox::Refresh() {
  assert(tiles_[0] != nullptr);
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
  Explain(&size_, why_not, tr("How far the dot moves at most."));
  Explain(&cycle_, why_not, tr("Frames one cycle takes; 24 is a second."));
  Explain(&angle_,
          is_shiver ? tr("A shiver shakes every way, not one.") : why_not,
          tr("The way the dot moves: 0 is right, 90 is down."));
  Explain(&delay_, why_not,
          tr("How late in its cycle the dot starts, 0 to 1. Give dots "
             "in a row rising delays for a ripple."));
  ShowTiles(tiles_, look, no_dot);
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
