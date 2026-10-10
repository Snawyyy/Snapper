#ifndef SNAPPER_ANIM_DRAG_SPRING_H_
#define SNAPPER_ANIM_DRAG_SPRING_H_

#include <QPointF>

#include "model/doll.h"

namespace snapper {

// The lone spring of a drag node (kind Drag): how hard it is pulled
// toward where its point is carried each frame, and how much of its
// speed it loses. More lag pulls softer; more bounce keeps more speed.
// Lag near 0 lands on its point every frame, with no jump as it nears 0.
struct DragSpring final {
  double pull = 1.0;
  double damping = 1.0;
};

DragSpring DragSpringOf(const DragNode& node);

// Moves a spring at at with speed one frame toward goal.
void StepDragSpring(const DragSpring& spring, QPointF goal, QPointF* at,
                    QPointF* speed);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DRAG_SPRING_H_
