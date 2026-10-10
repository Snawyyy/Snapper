#ifndef SNAPPER_ANIM_MESH_CARRY_H_
#define SNAPPER_ANIM_MESH_CARRY_H_

#include <QPointF>
#include <QSize>

#include <vector>

#include "model/doll.h"

namespace snapper {

// How many dots' settings (rubber reach, drag or physics node, motion)
// found a new dot when a warp was made again, and how many did not.
struct CarryCount final {
  int carried = 0;
  int dropped = 0;

  CarryCount& operator+=(const CarryCount& other) {
    carried += other.carried;
    dropped += other.dropped;
    return *this;
  }
  bool operator==(const CarryCount&) const = default;
};

// from's piece with its warp swapped for fresh (over a drawing of
// size; from's own was over from_size): each dot's settings go to the
// new dot nearest its spot, if one is within a cell of it (the old
// warp's cell or the new one's, whichever is bigger) and has not taken
// another's; the rest are dropped. Cloth (physics nodes) covers an
// area, so every other new dot whose nearest old dot was one becomes
// one too. Written to to.
CarryCount CarryDots(const RigPiece& from, QSize from_size,
                     const WarpGrid& fresh, QSize size, RigPiece* to);

// A key's warp offsets on old (over old_size) as offsets on fresh (over
// size): each new dot takes the push found at its spot on the old warp,
// so the key keeps its shape. Empty offsets stay empty.
std::vector<QPointF> CarryOffsets(const WarpGrid& old, QSize old_size,
                                  const std::vector<QPointF>& offsets,
                                  const WarpGrid& fresh, QSize size);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MESH_CARRY_H_
