#ifndef SNAPPER_ANIM_CLOTH_H_
#define SNAPPER_ANIM_CLOTH_H_

#include <QPointF>

#include <vector>

#include "anim/collider.h"
#include "model/doll.h"

namespace snapper {

// The five settings of a physics node, each 0 to 1 (see DragNode).
struct ClothSettings final {
  double stretch = 0.2;
  double inertia = 0.6;
  double air = 0.5;
  double gravity = 0.3;
  double spring = 0.2;
};

// The settings a physics drag node carries.
ClothSettings ClothSettingsOf(const DragNode& node);

// The farthest a physics point may stray from where the body carries
// it, in shot pixels, so a point nothing holds does not fall forever.
inline constexpr double kMaxClothDrift = 400.0;

// The points of one piece's cloth, in shot space. at and speed hold one
// entry per physics node; points names each node's grid point.
struct ClothPiece final {
  WarpGrid grid;
  std::vector<int> points;
  std::vector<ClothSettings> settings;
  std::vector<QPointF> at;
  std::vector<QPointF> speed;
  // Outlines that push the nodes out, in shot space, and where they
  // stood a frame before (empty on the first frame).
  std::vector<ColliderEdge> edges;
  std::vector<ColliderEdge> edges_before;
};

// Runs one frame of the cloth of piece. carried holds every point of
// the piece's grid as the body carries it this frame, in shot space.
// Physics nodes next to each other on the grid are tied like cloth and
// may not stretch past the length they have in carried (stretch softens
// that); the grid points that are not physics nodes are pins that
// follow carried exactly. Each node keeps some of its speed (inertia,
// slowed by air), is pulled down (gravity) and back to its carried
// place (spring), and the ties then pull it in. A node that ends up
// behind an edge is then pushed out to it (one-sided: nothing holds or
// pulls a node that is outside or moving away). The cloth between two
// nodes is kept out too: when an outline has come across it since the
// last frame (the leg turning up through the hem), the two nodes are
// pushed out together, though cloth that already lay across an outline
// (a skirt drawn over the leg) is left as drawn. Deterministic: the
// same input gives the same output.
void StepCloth(const std::vector<QPointF>& carried, ClothPiece* piece);

// Puts every node of piece at its carried place, at rest: the cloth as
// drawn, which is where it stands on the shot's first frame.
void RestCloth(const std::vector<QPointF>& carried, ClothPiece* piece);

}  // namespace snapper

#endif  // SNAPPER_ANIM_CLOTH_H_
