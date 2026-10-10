#include "anim/drag_spring.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// Lag below this eases into no trail at all.
constexpr double kSnapLag = 0.1;

double Mix(double from, double to, double amount) {
  assert(std::isfinite(amount));
  assert(std::isfinite(from) && std::isfinite(to));
  return from + (to - from) * amount;
}

}  // namespace

DragSpring DragSpringOf(const DragNode& node) {
  assert(std::isfinite(node.lag));
  assert(std::isfinite(node.bounce));
  const double soft_pull = Mix(0.5, 0.04, node.lag);
  const double give = Mix(1.0, 0.08, node.bounce);
  const double soft_damping = 2.0 * give * std::sqrt(soft_pull);
  // Below kSnapLag the spring blends into one with pull and damping 1,
  // which lands on its point every frame.
  const double snap = std::clamp(1.0 - node.lag / kSnapLag, 0.0, 1.0);
  return {Mix(soft_pull, 1.0, snap), Mix(soft_damping, 1.0, snap)};
}

void StepDragSpring(const DragSpring& spring, QPointF goal, QPointF* at,
                    QPointF* speed) {
  assert(at != nullptr && speed != nullptr);
  assert(std::isfinite(goal.x() + goal.y()));
  *speed += (goal - *at) * spring.pull - *speed * spring.damping;
  *at += *speed;
}

}  // namespace snapper
