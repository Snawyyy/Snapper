#include "model/node_physics.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace snapper {
namespace {

// The wobble period at firmness 0 and at firmness 1, in frames.
constexpr double kLongestPeriod = 24.0;
constexpr double kShortestPeriod = 4.0;
// The jiggles at wobble 1.
constexpr double kMostJiggles = 6.0;
// A jiggle is visible while it is above this share of the first swing.
constexpr double kVisible = 0.1;
// How far droop 1 sags, in cells per unit of turn from upright. Sag is
// the knob times this whatever the firmness, so every step of droop
// shows the same.
constexpr double kMostSag = 0.3;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

double Unit(double value) {
  assert(kMostSquish < 1.0);
  assert(kMostSag > 0.0);
  return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

// The damping (1 settles without a swing, toward 0 rings on) that
// makes jiggles visible swings before settling.
double DampingOf(double jiggles) {
  assert(jiggles >= 0.0);
  assert(std::isfinite(jiggles));
  const double fade = -std::log(kVisible);
  return fade / std::hypot(kTwoPi * jiggles, fade);
}

// How much bigger a squash shows at squish 1 than at 0: a squash
// springs back as hard as the share it does not keep, and shows as far
// as one over the root of that.
double MostSquashShown() {
  assert(kMostSquish > 0.0);
  assert(kMostSquish < 1.0);
  return 1.0 / std::sqrt(1.0 - kMostSquish);
}

// How a damped spring rings: its speed (radians per frame, with no
// damping) and its damping.
struct Ring final {
  double speed = 0.0;
  double damping = 1.0;
};

// The ring of an old jelly node, from its pull back per frame and the
// share of speed it kept: each frame it moved on by its kept speed and
// was then pulled part of the way back.
Ring OldRing(double pull, double keep) {
  assert(pull >= 0.0 && pull < 1.0);
  assert(keep >= 0.0 && keep < 1.0);
  const double det = (1.0 - pull) * keep;
  const double trace = (1.0 - pull) * (1.0 + keep);
  const double disc = trace * trace - 4.0 * det;
  const bool is_ringing = disc < 0.0;
  const double radius = std::sqrt(det);
  const double turn =
      is_ringing ? std::acos(std::clamp(trace / (2.0 * radius), -1.0, 1.0))
                 : 0.0;
  const double slowest = is_ringing ? radius : (trace + std::sqrt(disc)) / 2.0;
  const double decay = slowest > 1e-12 ? -std::log(slowest) : 30.0;
  const double speed = std::hypot(turn, decay);
  const bool is_moving = speed > 1e-9;
  return {speed, is_moving ? decay / speed : 1.0};
}

}  // namespace

double JellyPeriod(double firmness) {
  assert(kShortestPeriod < kLongestPeriod);
  assert(kShortestPeriod > 2.0);
  return kLongestPeriod *
         std::pow(kShortestPeriod / kLongestPeriod, Unit(firmness));
}

double JellyJiggles(double wobble) {
  assert(kMostJiggles > 0.0);
  assert(kVisible > 0.0 && kVisible < 1.0);
  return kMostJiggles * Unit(wobble);
}

JellyMotion JellyMotionOf(const DragNode& node) {
  assert(node.kind == NodeKind::kJelly);
  assert(node.point >= 0);
  const double speed = kTwoPi / JellyPeriod(node.spring);
  const double damping = DampingOf(JellyJiggles(node.inertia));
  // The frame-to-frame step of a spring ringing at speed with damping:
  // it shrinks by radius and turns by turn each frame.
  const double radius = std::exp(-damping * speed);
  const double turn =
      speed * std::sqrt(std::max(0.0, 1.0 - damping * damping));
  JellyMotion motion;
  motion.keep = radius * radius;
  motion.pull = 1.0 + radius * radius - 2.0 * radius * std::cos(turn);
  const double shown = 1.0 + (MostSquashShown() - 1.0) * Unit(node.stretch);
  motion.squish = 1.0 - 1.0 / (shown * shown);
  motion.fall = Unit(node.gravity) * kMostSag * motion.pull;
  return motion;
}

DragNode JellyFromOld(DragNode node) {
  assert(node.kind == NodeKind::kJelly);
  assert(kSpringPull < 1.0);
  const double pull = Unit(node.spring) * kSpringPull;
  const double keep =
      Unit(node.inertia) * std::max(0.0, kKeep - kAirDrag * Unit(node.air));
  const Ring ring = OldRing(pull, keep);
  const bool is_moving = ring.speed > 1e-9;
  const double period = is_moving ? kTwoPi / ring.speed : kLongestPeriod;
  const double fade = -std::log(kVisible);
  const double jiggles =
      ring.damping > 1e-9
          ? fade * std::sqrt(std::max(0.0, 1.0 - ring.damping * ring.damping)) /
                (kTwoPi * ring.damping)
          : kMostJiggles;
  const double shown =
      1.0 / std::sqrt(1.0 - Unit(node.stretch) * kMostSquish);
  const double sag = pull > 1e-9
                         ? (1.0 - pull) * Unit(node.gravity) * kGravityPull /
                               pull
                         : kMostSag;
  node.spring = Unit(std::log(kLongestPeriod / period) /
                     std::log(kLongestPeriod / kShortestPeriod));
  node.inertia = Unit(jiggles / kMostJiggles);
  node.stretch = Unit((shown - 1.0) / (MostSquashShown() - 1.0));
  node.gravity = Unit(sag / kMostSag);
  node.air = DragNode{}.air;
  return node;
}

}  // namespace snapper
