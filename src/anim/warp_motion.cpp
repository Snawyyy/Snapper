#include "anim/warp_motion.h"

#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/jitter.h"

namespace snapper {
namespace {

constexpr double kTurn = 2.0 * std::numbers::pi;
// How many fresh shakes a shiver makes in one cycle.
constexpr int kShakesPerCycle = 12;

// How far through its cycle motion is at frame, 0 up to 1.
double Phase(const WarpMotion& motion, Frame frame) {
  assert(motion.cycle >= kMinWarpCycle);
  assert(frame.index() >= 0);
  return static_cast<double>(frame.index() % motion.cycle) / motion.cycle;
}

// Where point sits on grid, 0 to 1 across and down. Never divides by
// zero: a grid that is not on has no points, so no point can reach
// here, and a loaded half grid such as 3 by 0 is not on.
QPointF Spot(WarpGrid grid, int point) {
  assert(grid.columns > 0 && grid.rows > 0);
  assert(point >= 0 && point < grid.PointCount());
  const int column = point % (grid.columns + 1);
  const int row = point / (grid.columns + 1);
  const QPointF spot(static_cast<double>(column) / grid.columns,
                     static_cast<double>(row) / grid.rows);
  assert(spot.x() >= 0.0 && spot.x() <= 1.0);
  assert(spot.y() >= 0.0 && spot.y() <= 1.0);
  return spot;
}

// How far spot is from edge, 0 on it to 1 on the far side.
double Along(QPointF spot, WarpEdge edge) {
  assert(spot.x() >= 0.0 && spot.x() <= 1.0);
  assert(spot.y() >= 0.0 && spot.y() <= 1.0);
  switch (edge) {
    case WarpEdge::kTop:
      return spot.y();
    case WarpEdge::kBottom:
      return 1.0 - spot.y();
    case WarpEdge::kLeft:
      return spot.x();
    case WarpEdge::kRight:
      return 1.0 - spot.x();
  }
  assert(false);
  return 0.0;
}

// Sideways to edge: the way something hanging from it swings.
QPointF Across(WarpEdge edge) {
  const bool is_level = edge == WarpEdge::kTop || edge == WarpEdge::kBottom;
  return is_level ? QPointF(1.0, 0.0) : QPointF(0.0, 1.0);
}

// A ripple that leaves the anchor edge and runs to the far side,
// swinging wider as it goes, like hair or a flag.
QPointF Wave(const WarpMotion& motion, double phase, QPointF spot) {
  const double along = Along(spot, motion.edge);
  const double swing = std::sin(kTurn * (phase - along));
  return Across(motion.edge) * (motion.size * along * swing);
}

// The whole piece swings to and fro together from the anchor edge,
// bending most at the free end, like a skirt or a tail.
QPointF Sway(const WarpMotion& motion, double phase, QPointF spot) {
  const double along = Along(spot, motion.edge);
  const double swing = std::sin(kTurn * phase);
  return Across(motion.edge) * (motion.size * along * along * swing);
}

// From the middle of the drawing out to spot, -1 to 1 each way.
QPointF Outward(QPointF spot) {
  assert(spot.x() >= 0.0 && spot.x() <= 1.0);
  assert(spot.y() >= 0.0 && spot.y() <= 1.0);
  return (spot - QPointF(0.5, 0.5)) * 2.0;
}

// A soft bump of width around centre on the cycle, 1 at its top.
double Bump(double phase, double centre, double width) {
  assert(width > 0.0);
  assert(phase >= 0.0 && phase < 1.0);
  const double from = (phase - centre) / width;
  return std::exp(-from * from);
}

// Two quick swells and a rest, lub-dub, like a heart.
QPointF Pulse(const WarpMotion& motion, double phase, QPointF spot) {
  const double beat =
      Bump(phase, 0.08, 0.05) + 0.6 * Bump(phase, 0.28, 0.05);
  return Outward(spot) * (motion.size * beat);
}

// Swells out from the middle and falls back, smooth all the way.
QPointF Breathe(const WarpMotion& motion, double phase, QPointF spot) {
  const double swell = (1.0 - std::cos(kTurn * phase)) / 2.0;
  return Outward(spot) * (motion.size * swell);
}

// Every dot jumps to its own small random spot, a fresh one
// kShakesPerCycle times a cycle; the same frame always shakes the same.
QPointF Shiver(const WarpMotion& motion, int point, Frame frame) {
  assert(point >= 0 && point < kMaxWarpPoints);
  assert(frame.index() >= 0);
  const int shake = frame.index() * kShakesPerCycle / motion.cycle;
  const int step = shake * kMaxWarpPoints + point;
  return QPointF(Jitter(step, 0), Jitter(step, 1)) * motion.size;
}

}  // namespace

bool HangsFromEdge(WarpMotionKind kind) {
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kWarpMotionKindCount);
  switch (kind) {
    case WarpMotionKind::kNone:
    case WarpMotionKind::kPulse:
    case WarpMotionKind::kBreathe:
    case WarpMotionKind::kShiver:
      return false;
    case WarpMotionKind::kWave:
    case WarpMotionKind::kSway:
      return true;
  }
  assert(false);
  return false;
}

std::vector<QPointF> MotionPushes(QSize size, WarpGrid grid,
                                  const WarpMotion& motion, Frame frame) {
  assert(frame.index() >= 0);
  assert(motion.cycle >= kMinWarpCycle && motion.cycle <= kMaxWarpCycle);
  std::vector<QPointF> pushes(static_cast<size_t>(grid.PointCount()));
  const bool is_moving = motion.IsOn() && grid.IsOn() && !size.isEmpty();
  if (!is_moving) {
    return pushes;
  }
  const double phase = Phase(motion, frame);
  for (int point = 0; point < grid.PointCount(); ++point) {
    const QPointF spot = Spot(grid, point);
    QPointF& push = pushes[static_cast<size_t>(point)];
    switch (motion.kind) {
      case WarpMotionKind::kNone:
        break;
      case WarpMotionKind::kWave:
        push = Wave(motion, phase, spot);
        break;
      case WarpMotionKind::kPulse:
        push = Pulse(motion, phase, spot);
        break;
      case WarpMotionKind::kBreathe:
        push = Breathe(motion, phase, spot);
        break;
      case WarpMotionKind::kSway:
        push = Sway(motion, phase, spot);
        break;
      case WarpMotionKind::kShiver:
        push = Shiver(motion, point, frame);
        break;
    }
  }
  assert(pushes.size() <= static_cast<size_t>(kMaxWarpPoints));
  return pushes;
}

}  // namespace snapper
