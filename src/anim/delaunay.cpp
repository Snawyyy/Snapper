#include "anim/delaunay.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <array>
#include <utility>

namespace snapper {
namespace {

// A triangle being built: its corners, running clockwise on a screen,
// and the triangle across each side (corner k to corner k + 1), or -1.
struct Built final {
  std::array<int, 3> v{};
  std::array<int, 3> next{-1, -1, -1};
};

// Twice the signed area: above 0 when a, b, c run clockwise on screen.
double Turn(QPointF a, QPointF b, QPointF c) {
  assert(std::isfinite(a.x() + a.y() + b.x() + b.y()));
  assert(std::isfinite(c.x() + c.y()));
  return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

// Whether d is inside the circle through a, b, c (which turn above 0).
bool IsInCircle(QPointF a, QPointF b, QPointF c, QPointF d) {
  assert(std::isfinite(a.x() + b.x() + c.x() + d.x()));
  assert(std::isfinite(a.y() + b.y() + c.y() + d.y()));
  const QPointF ad = a - d;
  const QPointF bd = b - d;
  const QPointF cd = c - d;
  const double a2 = ad.x() * ad.x() + ad.y() * ad.y();
  const double b2 = bd.x() * bd.x() + bd.y() * bd.y();
  const double c2 = cd.x() * cd.x() + cd.y() * cd.y();
  return a2 * (bd.x() * cd.y() - cd.x() * bd.y()) +
             b2 * (cd.x() * ad.y() - ad.x() * cd.y()) +
             c2 * (ad.x() * bd.y() - bd.x() * ad.y()) >
         0.0;
}

// The three points of a triangle big enough to hold every point,
// turning above 0.
std::vector<QPointF> Enclosing(const std::vector<QPointF>& points) {
  assert(!points.empty());
  assert(points.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  const QRectF box = BoundsOf(points);
  const double d = std::max({box.width(), box.height(), 1.0}) * 10.0;
  const QPointF c = box.center();
  return {QPointF(c.x() - 20.0 * d, c.y() - d),
          QPointF(c.x() + 20.0 * d, c.y() - d),
          QPointF(c.x(), c.y() + 20.0 * d)};
}

// The triangles as they are built, flipped to stay fat as each point
// comes in.
class Builder final {
 public:
  explicit Builder(std::vector<QPointF> at) : at_(std::move(at)) {
    assert(at_.size() >= 3);
    assert(Turn(P(at_.size() - 3), P(at_.size() - 2), P(at_.size() - 1)) >
           0.0);
    const int n = static_cast<int>(at_.size());
    built_.push_back({{n - 3, n - 2, n - 1}, {-1, -1, -1}});
  }
  void Insert(int point);
  const std::vector<Built>& built() const { return built_; }

 private:
  QPointF P(size_t i) const { return at_[i]; }
  QPointF P(int i) const { return at_[static_cast<size_t>(i)]; }
  int Locate(QPointF point) const;
  // Points the side of triangle tri that runs b to a at by instead.
  void Relink(int tri, int a, int b, int by);
  // Flips the side opposite the new point (its corner 2) of tri when
  // the triangle across holds it in its circle; true when it did.
  bool Flip(int tri, int point);

  std::vector<QPointF> at_;
  std::vector<Built> built_;
};

int Builder::Locate(QPointF point) const {
  assert(!built_.empty());
  assert(std::isfinite(point.x() + point.y()));
  int best = 0;
  double best_least = -1e300;
  for (size_t t = 0; t < built_.size(); ++t) {
    const auto& v = built_[t].v;
    const double least =
        std::min({Turn(P(v[0]), P(v[1]), point), Turn(P(v[1]), P(v[2]), point),
                  Turn(P(v[2]), P(v[0]), point)});
    const bool is_better = least > best_least;
    if (is_better) {
      best_least = least;
      best = static_cast<int>(t);
    }
    const bool is_inside = least >= 0.0;
    if (is_inside) {
      return static_cast<int>(t);
    }
  }
  return best;
}

void Builder::Relink(int tri, int a, int b, int by) {
  assert(by >= 0);
  assert(a != b);
  const bool is_real = tri >= 0;
  if (!is_real) {
    return;
  }
  Built& t = built_[static_cast<size_t>(tri)];
  for (size_t k = 0; k < 3; ++k) {
    const bool is_side = t.v[k] == b && t.v[(k + 1) % 3] == a;
    if (is_side) {
      t.next[k] = by;
    }
  }
}

bool Builder::Flip(int tri, int point) {
  assert(tri >= 0 && static_cast<size_t>(tri) < built_.size());
  assert(built_[static_cast<size_t>(tri)].v[2] == point);
  const Built t = built_[static_cast<size_t>(tri)];
  const int across = t.next[0];
  const bool has_across = across >= 0;
  if (!has_across) {
    return false;
  }
  const int x = t.v[0];
  const int y = t.v[1];
  const Built u = built_[static_cast<size_t>(across)];
  size_t j = 0;
  for (size_t k = 0; k < 3; ++k) {
    j = u.v[k] == y && u.v[(k + 1) % 3] == x ? k : j;
  }
  const int q = u.v[(j + 2) % 3];
  // Only a convex pair is flipped, so rounding near the huge outer
  // corners can never turn a triangle over.
  const bool is_convex = Turn(P(x), P(q), P(point)) > 0.0 &&
                         Turn(P(q), P(y), P(point)) > 0.0;
  const bool is_in =
      is_convex && IsInCircle(P(x), P(y), P(point), P(q));
  if (!is_in) {
    return false;
  }
  const int c = u.next[(j + 1) % 3];
  const int d = u.next[(j + 2) % 3];
  built_[static_cast<size_t>(tri)] = {{x, q, point}, {c, across, t.next[2]}};
  built_[static_cast<size_t>(across)] = {{q, y, point},
                                         {d, t.next[1], tri}};
  Relink(c, x, q, tri);
  Relink(t.next[1], y, point, across);
  return true;
}

void Builder::Insert(int point) {
  assert(point >= 0 && static_cast<size_t>(point) < at_.size());
  assert(built_.size() <= 2 * at_.size());
  const int t0 = Locate(P(point));
  const Built t = built_[static_cast<size_t>(t0)];
  const int a = t.v[0];
  const int b = t.v[1];
  const int c = t.v[2];
  const int t1 = static_cast<int>(built_.size());
  const int t2 = t1 + 1;
  built_[static_cast<size_t>(t0)] = {{a, b, point}, {t.next[0], t1, t2}};
  built_.push_back({{b, c, point}, {t.next[1], t2, t0}});
  built_.push_back({{c, a, point}, {t.next[2], t0, t1}});
  Relink(t.next[1], b, c, t1);
  Relink(t.next[2], c, a, t2);
  std::vector<int> todo{t0, t1, t2};
  const size_t most = 8 * at_.size() + 16;
  for (size_t step = 0; step < most && !todo.empty(); ++step) {
    const int tri = todo.back();
    todo.pop_back();
    const bool is_flipped = Flip(tri, point);
    if (is_flipped) {
      todo.push_back(tri);
      todo.push_back(built_[static_cast<size_t>(tri)].next[1]);
    }
  }
}

}  // namespace

QRectF BoundsOf(const std::vector<QPointF>& points) {
  assert(!points.empty());
  assert(points.size() <= static_cast<size_t>(4 * kMaxDelaunayPoints));
  double left = points[0].x();
  double right = left;
  double top = points[0].y();
  double bottom = top;
  for (const QPointF& point : points) {
    left = std::min(left, point.x());
    right = std::max(right, point.x());
    top = std::min(top, point.y());
    bottom = std::max(bottom, point.y());
  }
  return QRectF(QPointF(left, top), QPointF(right, bottom));
}

std::vector<MeshTriangle> Delaunay(const std::vector<QPointF>& points) {
  assert(points.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  assert(std::all_of(points.begin(), points.end(), [](QPointF p) {
    return std::isfinite(p.x()) && std::isfinite(p.y());
  }));
  std::vector<MeshTriangle> triangles;
  const bool is_enough = points.size() >= 3;
  if (!is_enough) {
    return triangles;
  }
  const int count = static_cast<int>(points.size());
  std::vector<QPointF> at = points;
  for (const QPointF& corner : Enclosing(points)) {
    at.push_back(corner);
  }
  Builder builder(std::move(at));
  for (int i = 0; i < count; ++i) {
    builder.Insert(i);
  }
  for (const Built& t : builder.built()) {
    const bool is_real =
        t.v[0] < count && t.v[1] < count && t.v[2] < count;
    if (is_real) {
      triangles.push_back(t.v);
    }
  }
  return triangles;
}

bool IsTiled(const std::vector<QPointF>& points,
             const std::vector<MeshTriangle>& triangles) {
  assert(points.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  assert(triangles.size() <= 2 * points.size() + 8);
  std::vector<MeshEdge> sides;
  sides.reserve(triangles.size() * 3);
  for (const MeshTriangle& t : triangles) {
    const bool is_turned =
        Turn(points[static_cast<size_t>(t[0])],
             points[static_cast<size_t>(t[1])],
             points[static_cast<size_t>(t[2])]) <= 1e-12;
    if (is_turned) {
      return false;
    }
    for (size_t k = 0; k < 3; ++k) {
      sides.emplace_back(t[k], t[(k + 1) % 3]);
    }
  }
  std::sort(sides.begin(), sides.end());
  return std::adjacent_find(sides.begin(), sides.end()) == sides.end();
}

}  // namespace snapper
