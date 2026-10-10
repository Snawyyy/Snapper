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

// How far outside an edge a pushed point is left, in shot pixels, so it
// reads as out and is not caught by the same edge again.
inline constexpr double kColliderClearance = 0.25;

// One pushing edge of a collider, in shot space: from a to b, with the
// outside of the shape along out (a unit step).
struct ColliderEdge final {
  QPointF a;
  QPointF b;
  QPointF out;
};

// Where points given in a piece's drawing pixels stand in shot space.
// grid_points is the piece's warped grid in shot space (empty when the
// piece has no warp grid), to_shot the map from its drawing pixels to
// shot space; a point is carried by the grid cell it sits in, so it
// bends with the piece.
std::vector<QPointF> ShotPoints(const std::vector<QPointF>& points,
                                WarpGrid grid, QSize size,
                                const std::vector<QPointF>& grid_points,
                                const QTransform& to_shot);

// Where the dots of collider are in shot space (see ShotPoints).
std::vector<QPointF> ShotDots(const Collider& collider, WarpGrid grid,
                              QSize size, const std::vector<QPointF>& points,
                              const QTransform& to_shot);

// The pushing edges of collider whose dots stand at shot_dots (in the
// order of collider.dots). An edge joins two dots only when they were
// neighbours on the ring, so deleted dots leave gaps. to_shot says how
// the drawing was carried: a mirror turns the outside around.
std::vector<ColliderEdge> ColliderEdges(const Collider& collider,
                                        const std::vector<QPointF>& shot_dots,
                                        const QTransform& to_shot);

// Where point ends up when ghost is where it would be had it kept its
// place against the collider as drawn (at rest). Walking from ghost to
// point, the last edge crossed tells: crossed going in, the point has
// come into the shape since rest and goes back out straight across the
// unbroken run of outline it came in through, to just outside its
// nearest spot; crossed going out, or not crossed at all, it stays. So
// what was drawn overlapping stays as drawn, a gap lets things
// through, and nothing outside is ever held or pulled.
QPointF PushedFromRest(const std::vector<ColliderEdge>& edges, QPointF ghost,
                       QPointF point);

// Where point, inside a collider, first comes out on its way straight
// to toward (a place outside): just past that edge. point itself when
// the way out crosses no edge.
QPointF OutToward(const std::vector<ColliderEdge>& edges, QPointF point,
                  QPointF toward);

}  // namespace snapper

#endif  // SNAPPER_ANIM_COLLIDER_H_
