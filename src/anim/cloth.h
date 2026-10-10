#ifndef SNAPPER_ANIM_CLOTH_H_
#define SNAPPER_ANIM_CLOTH_H_

#include <QPointF>

#include <vector>

#include "anim/doll_colliders.h"
#include "model/doll.h"

namespace snapper {

// The five settings of a physics node, each 0 (none of it) to 1 (a
// lot); see DragNode for what each does.
struct ClothSettings final {
  double stretch = 0.0;
  double inertia = 0.0;
  double air = 0.0;
  double gravity = 0.0;
  double spring = 0.0;
};

// The settings a physics drag node carries.
ClothSettings ClothSettingsOf(const DragNode& node);

// The farthest a physics point may stray from where the body carries
// it, in shot pixels, so a point nothing holds does not fall forever.
inline constexpr double kMaxClothDrift = 400.0;

// The points of one piece's cloth, in shot space. at, speed and
// body_speed hold one entry per physics node; points names each node's
// grid point.
struct ClothPiece final {
  WarpGrid grid;
  std::vector<int> points;
  std::vector<ClothSettings> settings;
  // Where each node is and how far it went last frame.
  std::vector<QPointF> at;
  std::vector<QPointF> speed;
  // How far the body carried each node's grid point since last frame.
  std::vector<QPointF> body_speed;
  // Which way is down in the drawing, in shot space (a unit step): the
  // drawing already hangs that way, so gravity pulls only by how far
  // shot space's down has turned from it.
  QPointF drawn_down{0.0, 1.0};
  // How long a grid cell is in shot space, which sizes gravity.
  double cell = 0.0;
  // The colliders that push the piece (see PushFieldsFor).
  std::vector<PushField> fields;
};

// Runs one frame of the cloth of piece. carried holds every point of
// the piece's grid as the body carries it this frame, in shot space.
// Each node is a weight that follows its point:
// - Inertia: how much of its own motion it keeps when the body speeds
//   up, stops or turns, so it lags, overshoots and follows through (0
//   moves with the body).
// - Air resistance: how fast that motion dies down (0 rings on longest).
// - Bounce: a spring back to its drawn place (0 none, so cloth just
//   hangs; 1 a firm jiggle).
// - Gravity: a pull down in shot space by how far the drawing has
//   turned from upright, so a drawing at rest stays as drawn and a doll
//   that falls over has its skirt and hair hang down.
// - Stretch: how far a node may get from the grid points around it (0
//   keeps its drawn distance, as cloth does; 1 a whole cell more).
// Nodes next to each other are tied like cloth; the other grid points
// are pins that follow carried exactly. A node a collider has come
// into since rest is pushed back out (PushedFromRest), one-sided.
// Deterministic: the same input gives the same output.
void StepCloth(const std::vector<QPointF>& carried, ClothPiece* piece);

// Puts every node of piece at its carried place, at rest: the cloth as
// drawn, which is where it stands on the shot's first frame.
void RestCloth(const std::vector<QPointF>& carried, ClothPiece* piece);

}  // namespace snapper

#endif  // SNAPPER_ANIM_CLOTH_H_
