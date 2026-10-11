#include "anim/cloth_parts.h"

#include <QPointF>

#include <cassert>
#include <cmath>
#include <vector>

#include "anim/grid_fold.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// The share of a collider's push a node keeps as speed: the rest is
// lost in the hit, so cloth rides on a leg instead of flying off it.
constexpr double kPushKick = 0.3;
// Rounds of turning back mesh triangles the nodes turned over.
constexpr int kFlipRounds = 4;

// Keeps a node within kMaxClothDrift of the place the body carries it,
// and puts it there if the numbers ever went bad.
QPointF Tethered(QPointF at, QPointF anchor) {
  assert(IsFinite(anchor));
  assert(kMaxClothDrift > 0.0);
  const bool is_bad = !IsFinite(at);
  if (is_bad) {
    return anchor;
  }
  const QPointF off = at - anchor;
  const double length = Length(off);
  const bool is_far = length > kMaxClothDrift;
  return is_far ? anchor + off * (kMaxClothDrift / length) : at;
}

// Keeps the cloth from turning over on itself: the grid with the nodes
// where they are must not fold against the grid as carried.
void UnfoldNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  std::vector<QPointF> grid = carried;
  for (size_t i = 0; i < piece->at.size(); ++i) {
    grid[static_cast<size_t>(piece->points[i])] = piece->at[i];
  }
  Unfold(piece->grid, carried, &grid);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    piece->at[i] = grid[static_cast<size_t>(piece->points[i])];
  }
}

// q mirrored across the line through u and v.
QPointF Mirrored(QPointF q, QPointF u, QPointF v) {
  assert(IsFinite(q) && IsFinite(u));
  assert(IsFinite(v));
  const QPointF d = v - u;
  const double length2 = d.x() * d.x() + d.y() * d.y();
  const bool is_point = length2 < 1e-12;
  if (is_point) {
    return q;
  }
  const QPointF foot =
      u + d * (((q - u).x() * d.x() + (q - u).y() * d.y()) / length2);
  return foot * 2.0 - q;
}

// Turns back every mesh triangle the cloth turned over against how the
// body carries it: a node corner is mirrored across the side facing
// it, which keeps every tie to that side as long as it was, so the
// sheet stays a sheet instead of crumpling. A grid keeps its own rules.
void UnflipNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const bool is_mesh = piece->grid.IsMesh();
  if (!is_mesh) {
    return;
  }
  std::vector<QPointF> at = carried;
  std::vector<int> node_of(carried.size(), -1);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    at[static_cast<size_t>(piece->points[i])] = piece->at[i];
    node_of[static_cast<size_t>(piece->points[i])] = static_cast<int>(i);
  }
  const auto turn = [](QPointF a, QPointF b, QPointF c) {
    return (b.x() - a.x()) * (c.y() - a.y()) -
           (b.y() - a.y()) * (c.x() - a.x());
  };
  for (int round = 0; round < kFlipRounds; ++round) {
    bool is_any = false;
    for (const MeshTriangle& t : piece->grid.mesh->triangles) {
      const auto p = [&t](size_t k) { return static_cast<size_t>(t[k]); };
      const double was = turn(carried[p(0)], carried[p(1)], carried[p(2)]);
      const double now = turn(at[p(0)], at[p(1)], at[p(2)]);
      const bool is_flipped = was * now < 0.0;
      for (size_t k = 0; k < 3 && is_flipped; ++k) {
        const bool is_node = node_of[p(k)] >= 0;
        if (is_node) {
          at[p(k)] = Mirrored(at[p(k)], at[p((k + 1) % 3)],
                              at[p((k + 2) % 3)]);
          is_any = true;
          break;
        }
      }
    }
    if (!is_any) {
      break;
    }
  }
  for (size_t i = 0; i < piece->at.size(); ++i) {
    piece->at[i] = at[static_cast<size_t>(piece->points[i])];
  }
}

}  // namespace

// Pushes every node a collider has come into back out, toward its own
// place (carried, which is already out): so it lands on the side of
// the outline it belongs on however deep a fast move took it.
void PushNodes(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(piece->at.size() == piece->points.size());
  for (const PushField& field : piece->fields) {
    for (size_t i = 0; i < piece->at.size(); ++i) {
      const size_t point = static_cast<size_t>(piece->points[i]);
      const QPointF at = piece->at[i];
      const bool is_in =
          PushedFromRest(field.edges, field.ghosts[point], at) != at;
      if (is_in) {
        piece->at[i] = OutToward(field.edges, at, carried[point]);
      }
    }
  }
}

// Moves node i one frame by its own motion, gravity, and home_pull (a
// share of the way back to where the body carries it).
void Move(const std::vector<QPointF>& carried, size_t i, double home_pull,
          ClothPiece* piece) {
  assert(piece != nullptr);
  assert(i < piece->at.size());
  const ClothSettings& s = piece->settings[i];
  const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
  const QPointF body = piece->body_speed[i];
  // Its place against the body before this frame's carry.
  const QPointF off = piece->at[i] - (anchor - body);
  // Its own motion on top of the body's, and gravity sized by the cell,
  // so a small drawing hangs as a big one does.
  QPointF own = (piece->speed[i] - body) * s.keep;
  const QPointF turned = QPointF(0.0, 1.0) - piece->drawn_down;
  own += turned * (s.fall * piece->cell) - off * home_pull;
  piece->at[i] = anchor + off + own;
}


void FinishStep(const std::vector<QPointF>& carried,
                const std::vector<QPointF>& before, bool is_untangling,
                ClothPiece* piece) {
  assert(piece != nullptr);
  assert(before.size() == piece->at.size());
  if (is_untangling) {
    UnflipNodes(carried, piece);
    UnfoldNodes(carried, piece);
  }
  const std::vector<QPointF> unpushed = piece->at;
  PushNodes(carried, piece);
  for (size_t i = 0; i < piece->at.size(); ++i) {
    const QPointF anchor = carried[static_cast<size_t>(piece->points[i])];
    piece->at[i] = Tethered(piece->at[i], anchor);
    const QPointF push = piece->at[i] - unpushed[i];
    const bool was_bad = !IsFinite(before[i]) || !IsFinite(unpushed[i]);
    piece->speed[i] = was_bad ? QPointF()
                              : unpushed[i] - before[i] + push * kPushKick;
  }
}

}  // namespace snapper
