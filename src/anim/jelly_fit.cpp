#include "anim/jelly_fit.h"

#include <QPointF>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <vector>

namespace snapper {
namespace {

// The least area (and so the most it is grown to keep area) of a blob.
constexpr double kLeastArea = 0.3;
// How strongly squash and stretch are pulled toward nothing when the
// spots do not say: a share of the weight they would get.
constexpr double kQuiet = 1e-3;

using Matrix3 = std::array<std::array<double, 3>, 3>;

double Det3(const Matrix3& m) {
  assert(std::isfinite(m[0][0] + m[1][1] + m[2][2]));
  assert(std::isfinite(m[0][1] + m[1][2] + m[2][0]));
  return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
         m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
         m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// m inverted (m is symmetric and made safe by its caller).
Matrix3 Inverse3(const Matrix3& m) {
  const double det = Det3(m);
  assert(std::abs(det) > 0.0);
  assert(std::isfinite(det));
  const double k = 1.0 / det;
  Matrix3 out{};
  out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * k;
  out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * k;
  out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * k;
  out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * k;
  out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * k;
  out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * k;
  out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * k;
  out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * k;
  out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * k;
  return out;
}

double Norm(const Swing& swing) {
  assert(std::isfinite(swing.a + swing.b));
  assert(std::isfinite(swing.c + swing.d));
  return std::sqrt(swing.a * swing.a + swing.b * swing.b +
                   swing.c * swing.c + swing.d * swing.d);
}

// swing with its squash and stretch kept under most, bending smoothly
// toward it instead of cutting off.
Swing WithinBend(const Swing& swing, double most) {
  assert(most > 0.0);
  assert(std::isfinite(swing.a + swing.d));
  const double norm = Norm(swing);
  const bool is_bent = norm > 1e-9;
  const double k = is_bent ? most * std::tanh(norm / most) / norm : 1.0;
  Swing out = swing;
  out.a *= k;
  out.b *= k;
  out.c *= k;
  out.d *= k;
  return out;
}

}  // namespace

QPointF SwingAt(const Swing& swing, QPointF q) {
  assert(std::isfinite(q.x() + q.y()));
  assert(std::isfinite(swing.shift.x() + swing.shift.y()));
  return swing.shift + QPointF(swing.a * q.x() + swing.b * q.y(),
                               swing.c * q.x() + swing.d * q.y());
}

Swing Plus(const Swing& x, const Swing& y) {
  assert(std::isfinite(x.a + y.a));
  assert(std::isfinite(x.shift.x() + y.shift.x()));
  return {x.shift + y.shift, x.a + y.a, x.b + y.b, x.c + y.c, x.d + y.d};
}

Swing Times(const Swing& x, double k) {
  assert(std::isfinite(k));
  assert(std::isfinite(x.a + x.d));
  return {x.shift * k, x.a * k, x.b * k, x.c * k, x.d * k};
}

Swing PulledBack(const Swing& swing, const Pull& pull) {
  assert(pull.shift >= 0.0 && pull.shift <= 2.0);
  assert(pull.squash >= 0.0 && pull.squash <= 2.0);
  const double hold = 1.0 - pull.squash;
  return {swing.shift * (1.0 - pull.shift), swing.a * hold, swing.b * hold,
          swing.c * hold, swing.d * hold};
}

Swing FitSwing(const std::vector<QPointF>& spots,
               const std::vector<double>& weights,
               const std::vector<QPointF>& values) {
  assert(spots.size() == weights.size());
  assert(spots.size() == values.size());
  Matrix3 normal{};
  std::array<double, 3> across{};
  std::array<double, 3> down{};
  double total = 0.0;
  for (size_t i = 0; i < spots.size(); ++i) {
    const std::array<double, 3> row{1.0, spots[i].x(), spots[i].y()};
    const double w = weights[i];
    total += w * w;
    for (size_t r = 0; r < 3; ++r) {
      for (size_t c = 0; c < 3; ++c) {
        normal[r][c] += w * w * row[r] * row[c];
      }
      across[r] += w * row[r] * values[i].x();
      down[r] += w * row[r] * values[i].y();
    }
  }
  normal[0][0] += kQuiet * total + 1e-12;
  normal[1][1] += kQuiet * total + 1e-12;
  normal[2][2] += kQuiet * total + 1e-12;
  const Matrix3 inverse = Inverse3(normal);
  std::array<double, 3> x{};
  std::array<double, 3> y{};
  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 3; ++c) {
      x[r] += inverse[r][c] * across[c];
      y[r] += inverse[r][c] * down[c];
    }
  }
  return {QPointF(x[0], y[0]), x[1], x[2], y[1], y[2]};
}

Swing Bounded(const Swing& swing, double keep, double most) {
  assert(keep >= 0.0 && keep <= 1.0);
  assert(most > 0.0 && most < 1.0);
  const Swing bent = WithinBend(swing, most);
  // The squash and stretch as a whole shape (the identity plus it), and
  // the same shape grown or shrunk to keep its area.
  const double area = (1.0 + bent.a) * (1.0 + bent.d) - bent.b * bent.c;
  const double grow = 1.0 / std::sqrt(std::max(area, kLeastArea));
  Swing kept = bent;
  kept.a = (1.0 + bent.a) * grow - 1.0;
  kept.b = bent.b * grow;
  kept.c = bent.c * grow;
  kept.d = (1.0 + bent.d) * grow - 1.0;
  return WithinBend(Plus(Times(bent, 1.0 - keep), Times(kept, keep)), most);
}

}  // namespace snapper
