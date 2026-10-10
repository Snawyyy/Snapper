#ifndef SNAPPER_UI_DOT_SHAPE_H_
#define SNAPPER_UI_DOT_SHAPE_H_

#include <QPointF>

#include <optional>

#include "model/doll.h"

namespace snapper {

// How a warp dot is drawn on the stage, by how it moves: a dot that
// stays is plain, a drag node a ring with a tail, cloth a diamond and
// jelly a blob that squishes. Pure, so tests can check the choices.
enum class DotShape { kPlain, kRing, kDiamond, kBlob };

// The shape for a dot whose node is kind; nullopt is a dot that stays.
DotShape ShapeOf(std::optional<NodeKind> kind);

// The tail of a ring: from the dot toward its keyed place, given how
// far the dot is shown from it (screen pixels). Zero when the dot has
// not moved off its key; never longer than kTailMax.
inline constexpr double kTailMax = 10.0;
QPointF TailOf(QPointF shown_from_keyed);

// A blob stretched along its motion and squashed across it, keeping
// its area. Round (1 by 1) when it is still or when animation is off.
struct Squish final {
  double along = 1.0;
  double across = 1.0;
  // Degrees, of the stretch axis.
  double angle = 0.0;
};
Squish SquishOf(QPointF shown_from_keyed, bool is_animated);

}  // namespace snapper

#endif  // SNAPPER_UI_DOT_SHAPE_H_
