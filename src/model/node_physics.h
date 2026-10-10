#ifndef SNAPPER_MODEL_NODE_PHYSICS_H_
#define SNAPPER_MODEL_NODE_PHYSICS_H_

#include "model/doll.h"

namespace snapper {

// The per-frame numbers behind a physics node's 0 to 1 settings. Cloth
// maps its settings straight onto them; jelly goes through four knobs
// spread evenly by feel. Files saved before the knobs kept jelly in the
// cloth numbers, so those are what JellyFromOld reads.

// The share of its own motion a cloth node keeps each frame with no air
// resistance (a little is always lost, so it settles), and what air
// resistance 1 takes off that.
inline constexpr double kKeep = 0.98;
inline constexpr double kAirDrag = 0.3;
// The share of a grid cell a cloth gravity of 1 pulls per frame per
// frame, per unit of turn away from upright (2 when upside down).
inline constexpr double kGravityPull = 0.02;
// The pull back to the drawn place a cloth bounce of 1 gives, per frame.
inline constexpr double kSpringPull = 0.3;
// The most of its squash a jelly blob keeps at squish 1: the rest pulls
// it back to its own shape, so even the softest jelly comes back.
inline constexpr double kMostSquish = 0.85;

// How a jelly node moves each frame.
struct JellyMotion final {
  // The share of its own speed it keeps.
  double keep = 0.0;
  // The share of the way to its place in the blob added to its speed.
  double pull = 0.0;
  // The share of its squash the blob's goal shape keeps (0 to
  // kMostSquish).
  double squish = 0.0;
  // How far it is pulled down, in cells per frame per frame, per unit of
  // turn away from upright.
  double fall = 0.0;
};

// The wobble period of a jelly node at firmness, in frames: about 24 at
// 0 to about 4 at 1, the same ratio for every step.
double JellyPeriod(double firmness);

// How many visible jiggles a jelly node makes before it settles at
// wobble: 0 at 0 to 6 at 1, in even steps.
double JellyJiggles(double wobble);

// The per-frame motion of a jelly node from its knobs: spring is
// Firmness, inertia is Wobble, stretch is Squish, gravity is Droop.
JellyMotion JellyMotionOf(const DragNode& node);

// A jelly node saved before the knobs (stretch, spring, inertia, air
// and gravity as cloth numbers) with the knobs that move nearest to it.
DragNode JellyFromOld(DragNode node);

}  // namespace snapper

#endif  // SNAPPER_MODEL_NODE_PHYSICS_H_
