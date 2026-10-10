#ifndef SNAPPER_ANIM_POKE_LAYER_H_
#define SNAPPER_ANIM_POKE_LAYER_H_

#include <QPointF>

#include <vector>

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/layer.h"

namespace snapper {

// The most virtual frames a poke runs: six seconds.
constexpr int kMaxPokeFrames = 6 * kFramesPerSecond;
// How much of a mouse move shoves the doll, and the most it may shove,
// in shot pixels. Small, so the dots wobble and the body barely moves.
constexpr double kPokeShare = 0.5;
constexpr double kPokeReach = 150.0;

// The shove a mouse move of move (shot pixels) gives the doll.
QPointF PokeShove(QPointF move);

// A copy of a doll layer for poking at frame at: every key before at is
// kept, the pose at at is held from there on, and the layer's own move
// is shoved by shoves[i] on frame at + 1 + i. Running the doll's
// springs on it shows how they answer a shove; shoves of zero let them
// settle. layer itself is not touched.
Layer PokeLayer(const Layer& layer, Frame at,
                const std::vector<QPointF>& shoves);

// How far apart two sets of poses put the grid points, at most, in
// drawing pixels; pieces or grids missing from one count as unmoved.
double PoseDistance(const PoseMap& a, const PoseMap& b);

}  // namespace snapper

#endif  // SNAPPER_ANIM_POKE_LAYER_H_
