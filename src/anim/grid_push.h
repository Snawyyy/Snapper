#ifndef SNAPPER_ANIM_GRID_PUSH_H_
#define SNAPPER_ANIM_GRID_PUSH_H_

#include <QPointF>

#include <vector>

#include "anim/doll_colliders.h"
#include "model/doll.h"

namespace snapper {

// The least a collider's push spreads to the neighbouring grid points,
// in grid cells, so a push bends the drawing smoothly even where the
// pushed point has no rubber reach of its own.
inline constexpr double kMinPushSpread = 2.0;

// The rubber reach of each of rig's grid points, or empty when it has
// none, as PushGrid takes it.
std::vector<double> ReachesOf(const RigPiece& rig);

// Pushes the points of a warp grid (shot space, row by row) out of the
// colliders in fields, as PushedFromRest says: every point, and every
// drawn edge between two neighbouring points, so an outline that comes
// up between two points lifts the edge too. Each point's push spreads
// to its neighbours by its rubber reach (reach, one per point, or
// empty; never less than kMinPushSpread) so the drawing bends instead
// of kinking, and a final pass makes sure no point or edge is left
// where a collider came into it. Only ever pushes out: with nothing
// coming into a collider since rest, nothing moves. Deterministic.
void PushGrid(WarpGrid grid, const std::vector<double>& reach,
              const std::vector<PushField>& fields,
              std::vector<QPointF>* points);

}  // namespace snapper

#endif  // SNAPPER_ANIM_GRID_PUSH_H_
