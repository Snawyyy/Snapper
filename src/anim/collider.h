#ifndef SNAPPER_ANIM_COLLIDER_H_
#define SNAPPER_ANIM_COLLIDER_H_

#include <QPointF>
#include <QSize>
#include <QTransform>

#include <vector>

#include "model/doll.h"

namespace snapper {

// The most edges that can push one piece at once.
inline constexpr size_t kMaxColliderEdges =
    static_cast<size_t>(kMaxColliderDots) * kMaxDollPieces;

// One pushing edge of a collider, in shot space: from a to b, with the
// outside of the shape along out (a unit step). A point behind the edge
// (on the other side of out) and within depth of it is pushed out.
struct ColliderEdge final {
  QPointF a;
  QPointF b;
  QPointF out;
  double depth = 0.0;
};

// Where the dots of collider are in shot space. points is the piece's
// warped grid in shot space (empty when the piece has no warp grid),
// to_shot the map from its drawing pixels to shot space; a dot is
// carried by the grid cell it sits in, so the outline bends with the
// piece.
std::vector<QPointF> ShotDots(const Collider& collider, WarpGrid grid,
                              QSize size, const std::vector<QPointF>& points,
                              const QTransform& to_shot);

// The pushing edges of collider whose dots stand at shot_dots (in the
// order of collider.dots). An edge joins two dots only when they were
// neighbours on the ring, so deleted dots leave gaps. to_shot says how
// the drawing was carried: a mirror turns the outside around, and its
// scale sets how deep the push reaches.
std::vector<ColliderEdge> ColliderEdges(const Collider& collider,
                                        const std::vector<QPointF>& shot_dots,
                                        const QTransform& to_shot);

// Where point ends up: on the nearest edge when it is behind one (no
// matter how it got there), else where it is. One-sided: a point
// outside, or moving away, is never held or pulled.
QPointF PushedOut(const std::vector<ColliderEdge>& edges, QPointF point);

}  // namespace snapper

#endif  // SNAPPER_ANIM_COLLIDER_H_
