#ifndef SNAPPER_TESTS_ANIM_LEG_SKIRT_H_
#define SNAPPER_TESTS_ANIM_LEG_SKIRT_H_

#include <QPointF>
#include <QTransform>

#include <cassert>
#include <cmath>
#include <variant>
#include <vector>

#include "anim/doll_pose.h"
#include "anim/sampler.h"
#include "anim/warp.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper::leg_skirt {

// A skirt 80 wide and 60 tall with a 4 by 3 grid (points 20 apart, the
// hem the last row at y 60) drawn over a thigh 20 wide and 80 long
// that hangs from the hip at (40, 35), so the thigh's top is drawn
// under the skirt as a real one is. The thigh's whole outline pushes.
inline constexpr int kAcross = 5;
inline constexpr int kPoints = 20;
inline constexpr int kHem = 15;

inline Collider ThighOutline() {
  Collider collider;
  // Clockwise with y down, the shape on the right: the top left to
  // right, down the right side, the bottom right to left, up the left.
  std::vector<QPointF> ring;
  for (int x = 0; x < 20; x += 10) {
    ring.emplace_back(x, 0);
  }
  for (int y = 0; y < 80; y += 10) {
    ring.emplace_back(20, y);
  }
  for (int x = 20; x > 0; x -= 10) {
    ring.emplace_back(x, 80);
  }
  for (int y = 80; y > 0; y -= 10) {
    ring.emplace_back(0, y);
  }
  collider.ring_size = static_cast<int>(ring.size());
  for (size_t i = 0; i < ring.size(); ++i) {
    collider.dots.push_back({ring[i], static_cast<int>(i)});
  }
  assert(collider.ring_size == 20);
  assert(collider.IsOn());
  return collider;
}

inline Doll LegAndSkirt() {
  Doll doll;
  doll.art.pieces = {{"thigh", {"t.png"}, 0, {30, 35}, {20, 80}},
                     {"skirt", {"s.png"}, 0, {0, 0}, {80, 60}}};
  doll.rig.pieces = {{"thigh", "", {10, 0}, 0, -1, {}},
                     {"skirt", "", {40, 0}, 1, -1, {4, 3}}};
  doll.rig.pieces[0].collider = ThighOutline();
  assert(doll.rig.pieces[1].warp.PointCount() == kPoints);
  assert(doll.rig.pieces[0].collider.IsOn());
  return doll;
}

// Makes the skirt's hem (or every row from row on) physics nodes.
inline void MakeCloth(Doll* doll, int row, const DragNode& tune = {}) {
  assert(doll != nullptr);
  assert(row >= 0 && row <= 3);
  for (int point = row * kAcross; point < kPoints; ++point) {
    DragNode node = tune;
    node.point = point;
    node.physics = true;
    doll->rig.pieces[1].drag_nodes.push_back(node);
  }
}

inline Layer Still() {
  Layer layer;
  layer.content = DollLayer{"Doll", {}, false};
  assert(layer.transform.keys.empty());
  assert(std::holds_alternative<DollLayer>(layer.content));
  return layer;
}

// The thigh turned by each of turns (degrees) on the frames given,
// eased linearly between.
inline Layer Kicking(const std::vector<std::pair<int, double>>& turns) {
  assert(!turns.empty());
  assert(turns.size() < 100);
  Layer layer = Still();
  auto& thigh = std::get<DollLayer>(layer.content).pieces["thigh"];
  for (const auto& [frame, turn] : turns) {
    PiecePose pose;
    pose.rotation = turn;
    SetKey(&thigh, {Frame(frame), pose, Ease::kLinear});
  }
  return layer;
}

// The skirt's grid points as shown, in shot (here doll) space.
inline std::vector<QPointF> Shown(const PoseMap& poses) {
  assert(poses.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto pose = poses.find("skirt");
  const std::vector<QPointF> warp =
      pose != poses.end() ? pose->second.warp : std::vector<QPointF>();
  std::vector<QPointF> points = WarpedPoints({80, 60}, {4, 3}, warp);
  assert(points.size() == static_cast<size_t>(kPoints));
  return points;
}

// Whether shot point p lies inside the thigh by more than slack (a
// slack below 0 grows the thigh), with the thigh where poses put it.
inline bool IsInThigh(const Doll& doll, const PoseMap& poses, QPointF p,
                      double slack) {
  assert(slack > -10.0 && slack < 10.0);
  assert(doll.art.pieces.size() == 2);
  bool is_invertible = false;
  const QPointF at = PieceTransforms(doll, poses)
                         .at("thigh")
                         .inverted(&is_invertible)
                         .map(p);
  assert(is_invertible);
  return at.x() > slack && at.x() < 20.0 - slack && at.y() > slack &&
         at.y() < 80.0 - slack;
}

// Every drawn edge of the skirt's grid, as pairs of points.
inline std::vector<std::pair<int, int>> GridEdges() {
  std::vector<std::pair<int, int>> edges;
  for (int point = 0; point < kPoints; ++point) {
    const bool has_right = point % kAcross < kAcross - 1;
    const bool has_below = point + kAcross < kPoints;
    if (has_right) {
      edges.emplace_back(point, point + 1);
    }
    if (has_below) {
      edges.emplace_back(point, point + kAcross);
    }
  }
  assert(edges.size() == 31);
  assert(edges.front().first == 0);
  return edges;
}

// How many spots of the skirt are inside the thigh now though they
// were clear of it as drawn: its points, and the spots along each drawn
// edge that lay wholly clear of the thigh as drawn (an edge the thigh
// was drawn passing under may keep crossing it). What a collider must
// never let happen.
inline int CameIn(const Doll& doll, const PoseMap& poses, double slack) {
  assert(slack >= 0.0);
  assert(doll.rig.pieces.size() == 2);
  const std::vector<QPointF> now = Shown(poses);
  const std::vector<QPointF> rest = Shown(PoseMap());
  constexpr int kSpots = 32;
  int count = 0;
  for (size_t i = 0; i < now.size(); ++i) {
    const bool was_in = IsInThigh(doll, PoseMap(), rest[i], 0.0);
    count += !was_in && IsInThigh(doll, poses, now[i], slack) ? 1 : 0;
  }
  for (const auto& [a, b] : GridEdges()) {
    const auto at = [a, b](const std::vector<QPointF>& points, int spot) {
      const double t = spot / static_cast<double>(kSpots);
      return points[static_cast<size_t>(a)] * (1.0 - t) +
             points[static_cast<size_t>(b)] * t;
    };
    bool was_clear = true;
    for (int spot = 0; spot <= kSpots; ++spot) {
      was_clear = was_clear && !IsInThigh(doll, PoseMap(), at(rest, spot),
                                          -0.5);
    }
    for (int spot = 0; spot <= kSpots && was_clear; ++spot) {
      count += IsInThigh(doll, poses, at(now, spot), slack) ? 1 : 0;
    }
  }
  return count;
}

}  // namespace snapper::leg_skirt

#endif  // SNAPPER_TESTS_ANIM_LEG_SKIRT_H_
