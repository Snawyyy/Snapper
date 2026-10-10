#include "anim/jelly.h"

#include <QPointF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "anim/cloth_parts.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// How far the area may be off before the fit stops making up all of
// it: a blob squeezed to nothing does not swell without end.
constexpr double kMostAreaChange = 4.0;
// The most of its squash a blob keeps at squish 1: the rest pulls it
// back to its own shape, so even the softest jelly comes back.
constexpr double kMostSquish = 0.85;

// A 2 by 2 matrix: [a b; c d].
struct Mat final {
  double a = 1.0;
  double b = 0.0;
  double c = 0.0;
  double d = 1.0;
};

double Det(const Mat& m) {
  assert(std::isfinite(m.a + m.b));
  assert(std::isfinite(m.c + m.d));
  return m.a * m.d - m.b * m.c;
}

Mat Times(const Mat& m, double k) {
  assert(std::isfinite(k));
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  return {m.a * k, m.b * k, m.c * k, m.d * k};
}

Mat Plus(const Mat& m, const Mat& n) {
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  assert(std::isfinite(n.a + n.b + n.c + n.d));
  return {m.a + n.a, m.b + n.b, m.c + n.c, m.d + n.d};
}

Mat Product(const Mat& m, const Mat& n) {
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  assert(std::isfinite(n.a + n.b + n.c + n.d));
  return {m.a * n.a + m.b * n.c, m.a * n.b + m.b * n.d,
          m.c * n.a + m.d * n.c, m.c * n.b + m.d * n.d};
}

QPointF Apply(const Mat& m, QPointF v) {
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  assert(std::isfinite(v.x() + v.y()));
  return {m.a * v.x() + m.b * v.y(), m.c * v.x() + m.d * v.y()};
}

// m scaled so its area change is area, or turn scaled so when m has
// none (flat or turned over).
Mat WithArea(const Mat& m, const Mat& turn, double area) {
  assert(area > 0.0);
  assert(std::isfinite(area));
  const double det = Det(m);
  const bool is_open = det > 1e-9 && std::isfinite(det);
  return is_open ? Times(m, std::sqrt(area / det))
                 : Times(turn, std::sqrt(area));
}

QPointF Middle(const std::vector<QPointF>& points) {
  assert(!points.empty());
  assert(points.size() <= static_cast<size_t>(kMaxWarpPoints));
  QPointF sum;
  for (const QPointF& p : points) {
    sum += p;
  }
  return sum / static_cast<double>(points.size());
}

// The squash and turn that best carries from onto to, both about
// their middles.
Mat FitOf(const std::vector<QPointF>& from, const std::vector<QPointF>& to) {
  assert(from.size() == to.size() && !from.empty());
  assert(from.size() <= static_cast<size_t>(kMaxWarpPoints));
  const QPointF from_mid = Middle(from);
  const QPointF to_mid = Middle(to);
  Mat pq{0.0, 0.0, 0.0, 0.0};
  Mat qq{0.0, 0.0, 0.0, 0.0};
  for (size_t k = 0; k < from.size(); ++k) {
    const QPointF q = from[k] - from_mid;
    const QPointF p = to[k] - to_mid;
    pq = Plus(pq, {p.x() * q.x(), p.x() * q.y(), p.y() * q.x(),
                   p.y() * q.y()});
    qq = Plus(qq, {q.x() * q.x(), q.x() * q.y(), q.y() * q.x(),
                   q.y() * q.y()});
  }
  const double qq_det = Det(qq);
  const bool is_spread = std::abs(qq_det) > 1e-9;
  return is_spread
             ? Product(pq, Times(Mat{qq.d, -qq.b, -qq.c, qq.a}, 1.0 / qq_det))
             : Mat{};
}

// The nearest pure turn to m.
Mat TurnOf(const Mat& m) {
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  assert(kMostAreaChange > 1.0);
  const double angle = std::atan2(m.c - m.b, m.a + m.d);
  return {std::cos(angle), -std::sin(angle), std::sin(angle),
          std::cos(angle)};
}

// The way the carried shape was squeezed least, as a unit step in shot
// space, and how lopsided the squeeze was: 0 when it shrank evenly, 1
// when it was all one way.
struct Squeeze final {
  QPointF free_way;
  double lopsided = 0.0;
};

Squeeze SqueezeOf(const Mat& drawn_to_carried) {
  const Mat& m = drawn_to_carried;
  assert(std::isfinite(m.a + m.b + m.c + m.d));
  assert(kMostAreaChange > 1.0);
  // The stretch of m (its turn left out): m transposed times m.
  const double p = m.a * m.a + m.c * m.c;
  const double q = m.a * m.b + m.c * m.d;
  const double r = m.b * m.b + m.d * m.d;
  const double spread = std::hypot((p - r) / 2.0, q);
  const double most = (p + r) / 2.0 + spread;
  const double least = std::max(0.0, (p + r) / 2.0 - spread);
  const QPointF one(q, most - p);
  const QPointF other(most - r, q);
  const QPointF way = Length(one) > Length(other) ? one : other;
  const QPointF carried_way = Apply(m, Length(way) > 1e-12 ? way
                                                          : QPointF(1, 0));
  const double length = Length(carried_way);
  const double total = std::sqrt(most) + std::sqrt(least);
  return {length > 1e-12 ? carried_way / length : QPointF(1.0, 0.0),
          total > 1e-12
              ? (std::sqrt(most) - std::sqrt(least)) / total
              : 0.0};
}

// The carried shape (about its middle) grown back to the drawn area:
// along the way it was squeezed least, so pinched at the sides it gets
// taller, and evenly when it shrank evenly.
std::vector<QPointF> Refilled(const std::vector<QPointF>& carried,
                              const ClothPiece& piece) {
  assert(static_cast<int>(carried.size()) == piece.grid.PointCount());
  assert(piece.drawn.size() == carried.size());
  const double area = ShapeArea(piece.grid, carried);
  const double drawn = ShapeArea(piece.grid, piece.drawn);
  const bool is_known = area > 1e-6 && drawn > 1e-6;
  const double grow =
      is_known ? std::clamp(drawn / area, 1.0 / kMostAreaChange,
                            kMostAreaChange)
               : 1.0;
  const Squeeze squeeze = SqueezeOf(FitOf(piece.drawn, carried));
  const QPointF w = squeeze.free_way;
  const Mat one_way{1.0 + (grow - 1.0) * w.x() * w.x(),
                    (grow - 1.0) * w.x() * w.y(),
                    (grow - 1.0) * w.x() * w.y(),
                    1.0 + (grow - 1.0) * w.y() * w.y()};
  const double lean = std::clamp(squeeze.lopsided * 4.0, 0.0, 1.0);
  const Mat bulge = WithArea(Plus(Times(one_way, lean),
                                  Times(Mat{}, (1.0 - lean) *
                                                   std::sqrt(grow))),
                             Mat{}, grow);
  const QPointF mid = Middle(carried);
  std::vector<QPointF> refilled;
  for (const QPointF& point : carried) {
    refilled.push_back(mid + Apply(bulge, point - mid));
  }
  return refilled;
}

}  // namespace

double ShapeArea(const WarpGrid& grid, const std::vector<QPointF>& points) {
  assert(static_cast<int>(points.size()) == grid.PointCount());
  assert(points.size() <= static_cast<size_t>(kMaxWarpPoints));
  double twice = 0.0;
  for (const MeshTriangle& t : WarpTriangles(grid)) {
    const QPointF a = points[static_cast<size_t>(t[0])];
    const QPointF b = points[static_cast<size_t>(t[1])];
    const QPointF c = points[static_cast<size_t>(t[2])];
    twice += (b.x() - a.x()) * (c.y() - a.y()) -
             (b.y() - a.y()) * (c.x() - a.x());
  }
  return std::abs(twice) / 2.0;
}

void StepJelly(const std::vector<QPointF>& carried, ClothPiece* piece) {
  assert(piece != nullptr);
  assert(static_cast<int>(carried.size()) == piece->grid.PointCount());
  const size_t count = piece->points.size();
  assert(piece->at.size() == count && piece->speed.size() == count &&
         piece->settings.size() == count && piece->body_speed.size() == count);
  const std::vector<QPointF> before = piece->at;
  double squish = 0.0;
  for (size_t i = 0; i < count; ++i) {
    // The fit, not the carried place, is home: no pull straight back.
    Move(carried, i, 0.0, piece);
    squish += piece->settings[i].stretch / static_cast<double>(count);
  }
  const bool has_drawn = piece->drawn.size() == carried.size();
  const std::vector<QPointF> home =
      has_drawn ? Refilled(carried, *piece) : carried;
  std::vector<QPointF> now = carried;
  for (size_t i = 0; i < count; ++i) {
    now[static_cast<size_t>(piece->points[i])] = piece->at[i];
  }
  // How the blob squashes and turns now, kept to the home area: squish
  // of the way between only turning and squashing freely.
  const Mat squash = FitOf(home, now);
  const Mat turn = TurnOf(squash);
  const double keep = std::clamp(squish, 0.0, 1.0) * kMostSquish;
  const Mat fit = WithArea(Plus(Times(WithArea(squash, turn, 1.0), keep),
                                Times(turn, 1.0 - keep)),
                           turn, 1.0);
  const QPointF home_mid = Middle(home);
  const QPointF now_mid = Middle(now);
  for (size_t i = 0; i < count; ++i) {
    const size_t point = static_cast<size_t>(piece->points[i]);
    const QPointF goal = now_mid + Apply(fit, home[point] - home_mid);
    piece->at[i] +=
        (goal - piece->at[i]) * (piece->settings[i].spring * kSpringPull);
  }
  FinishStep(carried, before, piece);
}

}  // namespace snapper
