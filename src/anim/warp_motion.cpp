#include "anim/warp_motion.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/jitter.h"
#include "anim/warp.h"

namespace snapper {
namespace {

constexpr double kTurn = 2.0 * std::numbers::pi;
// How many fresh shakes a shiver makes in one cycle.
constexpr int kShakesPerCycle = 12;
// How late, in cycles, a wave reaches the edge of its point's reach.
constexpr double kWaveLag = 0.5;
// Cycles added before taking the phase, so a delay never goes below the
// shot's first frame.
constexpr double kLead = 2.0;

// Cycles gone by at frame, started late by late cycles; never negative.
double Cycles(const PointMotion& motion, Frame frame, double late) {
  assert(motion.cycle >= kMinWarpCycle);
  assert(late >= 0.0 && late < kLead);
  const double cycles =
      static_cast<double>(frame.index()) / motion.cycle + kLead - late;
  assert(cycles > 0.0);
  return cycles;
}

// A soft bump of width around centre on the cycle, 1 at its top.
double Bump(double phase, double centre, double width) {
  assert(width > 0.0);
  assert(phase >= 0.0 && phase < 1.0);
  const double from = (phase - centre) / width;
  return std::exp(-from * from);
}

// How far along its direction kind is at phase, -1 to 1.
double Amount(WarpMotionKind kind, double phase) {
  assert(phase >= 0.0 && phase < 1.0);
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kWarpMotionKindCount);
  switch (kind) {
    case WarpMotionKind::kNone:
    case WarpMotionKind::kShiver:
      return 0.0;
    case WarpMotionKind::kWave:
    case WarpMotionKind::kSway:
      return std::sin(kTurn * phase);
    case WarpMotionKind::kPulse:
      // Two quick swells and a rest, lub-dub; they barely overlap, so
      // the beat never tops the lub.
      return Bump(phase, 0.08, 0.05) + 0.6 * Bump(phase, 0.28, 0.05);
    case WarpMotionKind::kBreathe:
      return (1.0 - std::cos(kTurn * phase)) / 2.0;
  }
  assert(false);
  return 0.0;
}

}  // namespace

QPointF MotionPush(const PointMotion& motion, Frame frame, double extra) {
  assert(frame.index() >= 0);
  assert(motion.size >= 0.0 && motion.size <= kMaxWarpMotionSize);
  const double cycles = Cycles(motion, frame, motion.delay + extra);
  const bool is_shiver = motion.kind == WarpMotionKind::kShiver;
  if (is_shiver) {
    // Its own small random spot, kShakesPerCycle fresh ones a cycle.
    const int shake = static_cast<int>(cycles * kShakesPerCycle);
    const int step = shake * kMaxWarpPoints + motion.point;
    return QPointF(Jitter(step, 0), Jitter(step, 1)) * motion.size;
  }
  const double whole = std::floor(cycles);
  const double phase = std::min(cycles - whole, std::nextafter(1.0, 0.0));
  const double turn = motion.angle * std::numbers::pi / 180.0;
  const QPointF way(std::cos(turn), std::sin(turn));
  return way * (motion.size * Amount(motion.kind, phase));
}

std::vector<QPointF> PointMotionPushes(WarpGrid grid,
                                       const PointMotion& motion,
                                       double reach, Frame frame) {
  assert(frame.index() >= 0);
  assert(motion.cycle >= kMinWarpCycle && motion.cycle <= kMaxWarpCycle);
  const int count = grid.PointCount();
  std::vector<QPointF> pushes(static_cast<size_t>(count));
  const bool is_moving = motion.kind != WarpMotionKind::kNone &&
                         motion.point >= 0 && motion.point < count;
  if (!is_moving) {
    return pushes;
  }
  const QPointF own = MotionPush(motion, frame);
  const bool is_wave = motion.kind == WarpMotionKind::kWave && reach > 0.0;
  for (int other = 0; other < count; ++other) {
    const double weight = PullWeight(grid, motion.point, other, reach);
    const bool is_reached = weight > 0.0;
    if (!is_reached) {
      continue;
    }
    const double late =
        is_wave ? kWaveLag * CellsApart(grid, motion.point, other) / reach
                : 0.0;
    pushes[static_cast<size_t>(other)] =
        (is_wave ? MotionPush(motion, frame, late) : own) * weight;
  }
  assert(pushes.size() <= static_cast<size_t>(kMaxWarpPoints));
  return pushes;
}

}  // namespace snapper
