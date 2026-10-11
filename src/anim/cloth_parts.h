#ifndef SNAPPER_ANIM_CLOTH_PARTS_H_
#define SNAPPER_ANIM_CLOTH_PARTS_H_

// The steps cloth and jelly share for their nodes: moving each by its
// own motion, keeping them out of colliders and from turning over, and
// working out their speed for the next frame.

#include <QPointF>

#include <cmath>
#include <vector>

#include "anim/cloth.h"

namespace snapper {

inline double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// A sum of two finite numbers far below overflow is finite.
inline bool IsFinite(QPointF p) { return std::isfinite(p.x() + p.y()); }

// Moves node i one frame by its own motion (speed kept), gravity, and
// home_pull: the share of the way back to where the body carries it.
void Move(const std::vector<QPointF>& carried, size_t i, double home_pull,
          ClothPiece* piece);

// Pushes every node a collider has come into back out, toward its own
// place (carried, which is already out).
void PushNodes(const std::vector<QPointF>& carried, ClothPiece* piece);

// The end of a frame: turns back what turned over (when is_untangling),
// pushes the nodes out of colliders, keeps them near the body, and sets
// each node's speed from where it was before the frame.
void FinishStep(const std::vector<QPointF>& carried,
                const std::vector<QPointF>& before, bool is_untangling,
                ClothPiece* piece);

}  // namespace snapper

#endif  // SNAPPER_ANIM_CLOTH_PARTS_H_
