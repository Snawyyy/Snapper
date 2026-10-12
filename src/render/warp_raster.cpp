#include "render/warp_raster.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include "anim/warp.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// Pixels on a shared triangle edge count for both triangles, so edges
// never leave a gap.
constexpr double kEdgeSlack = 1e-6;
// A warp may push a drawing at most this far past its own size.
constexpr int kMaxWarpedSide = 16384;
struct Triangle final {
  QPointF to[3];
  QPointF from[3];
};

// Nearest-neighbour read of a premultiplied image; outside is clear.
[[gnu::always_inline]] inline QRgb Sample(const QImage& image, QPointF at) {
  assert(image.format() == QImage::Format_ARGB32_Premultiplied);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const int x = static_cast<int>(std::floor(at.x()));
  const int y = static_cast<int>(std::floor(at.y()));
  const bool is_inside =
      x >= 0 && y >= 0 && x < image.width() && y < image.height();
  const auto* row = reinterpret_cast<const QRgb*>(
      image.constScanLine(is_inside ? y : 0));
  return is_inside ? row[x] : 0u;
}

// How far a barycentric weight moves per pixel to the right, and its
// value at the row's left edge (px = 0): weight = slope * px + base.
struct Line final {
  double slope;
  double base;
};

// The columns (in px, the target pixel's centre in the picture's space)
// where all three weights are inside the triangle, widened a little:
// the exact test is still made on each pixel, this only skips the
// ones that can't pass.
bool InsideColumns(const Line (&weights)[3], double* lo, double* hi) {
  assert(lo != nullptr && hi != nullptr);
  assert(kEdgeSlack > 0.0);
  *lo = -1e300;
  *hi = 1e300;
  for (const Line& line : weights) {
    const bool is_steady = std::abs(line.slope) < 1e-12;
    if (is_steady) {
      const bool is_out = line.base < -kEdgeSlack;
      if (is_out) {
        return false;
      }
      continue;
    }
    const double edge = (-kEdgeSlack - line.base) / line.slope;
    const bool is_lower_bound = line.slope > 0.0;
    *lo = is_lower_bound ? std::max(*lo, edge) : *lo;
    *hi = is_lower_bound ? *hi : std::min(*hi, edge);
  }
  return *lo <= *hi;
}

// The weights of a, b and c at row py, as lines over px.
void RowWeights(const Triangle& triangle, double area, double py,
                Line (*out)[3]) {
  assert(out != nullptr);
  assert(area != 0.0);
  const QPointF& a = triangle.to[0];
  const QPointF& b = triangle.to[1];
  const QPointF& c = triangle.to[2];
  const Line u{(b.y() - c.y()) / area,
               (b.x() * (c.y() - py) - c.x() * (b.y() - py)) / area};
  const Line v{(c.y() - a.y()) / area,
               (c.x() * (a.y() - py) - a.x() * (c.y() - py)) / area};
  (*out)[0] = u;
  (*out)[1] = v;
  (*out)[2] = {-(u.slope + v.slope), 1.0 - u.base - v.base};
}

// What f0, f1 and f2 (the value at each corner) mix to across a row.
Line Mixed(const Line (&weights)[3], double f0, double f1, double f2) {
  assert(std::isfinite(f0) && std::isfinite(f1) && std::isfinite(f2));
  assert(std::isfinite(weights[0].slope));
  return {f0 * weights[0].slope + f1 * weights[1].slope +
              f2 * weights[2].slope,
          f0 * weights[0].base + f1 * weights[1].base + f2 * weights[2].base};
}

// Draws one row of the triangle between columns x0 and x1. Inside a
// triangle the source position moves the same step for every pixel, so
// it is found once and stepped along.
void DrawRow(const QImage& source, const Triangle& triangle,
             const Line (&weights)[3], double origin_x, int x0, int x1,
             QRgb* row) {
  assert(row != nullptr);
  assert(x0 <= x1);
  const Line at_x = Mixed(weights, triangle.from[0].x(), triangle.from[1].x(),
                          triangle.from[2].x());
  const Line at_y = Mixed(weights, triangle.from[0].y(), triangle.from[1].y(),
                          triangle.from[2].y());
  for (int x = x0; x < x1; ++x) {
    const double px = x + 0.5 + origin_x;
    row[x] = Sample(source, QPointF(at_x.slope * px + at_x.base,
                                    at_y.slope * px + at_y.base));
  }
}

void DrawTriangle(const QImage& source, const Triangle& triangle,
                  QPointF origin, QImage* target) {
  assert(target != nullptr);
  assert(!source.isNull());
  const QPointF& a = triangle.to[0];
  const QPointF& b = triangle.to[1];
  const QPointF& c = triangle.to[2];
  const double area = (b.x() - a.x()) * (c.y() - a.y()) -
                      (c.x() - a.x()) * (b.y() - a.y());
  const bool is_flat = std::abs(area) < 1e-9;
  if (is_flat) {
    return;
  }
  // The target pixels the triangle can touch.
  const double left = std::min({a.x(), b.x(), c.x()});
  const double top = std::min({a.y(), b.y(), c.y()});
  const double right = std::max({a.x(), b.x(), c.x()});
  const double bottom = std::max({a.y(), b.y(), c.y()});
  const QRect box = QRectF(QPointF(left, top), QPointF(right, bottom))
                        .translated(-origin)
                        .toAlignedRect()
                        .intersected(target->rect());
  for (int y = box.top(); y < box.bottom() + 1; ++y) {
    Line weights[3];
    RowWeights(triangle, area, y + 0.5 + origin.y(), &weights);
    double lo = 0.0;
    double hi = 0.0;
    const bool is_reached = InsideColumns(weights, &lo, &hi);
    if (!is_reached) {
      continue;
    }
    // Pixels whose centre lies between lo and hi.
    const double first = std::ceil(lo - 0.5 - origin.x());
    const double last = std::floor(hi - 0.5 - origin.x()) + 1.0;
    const int x0 = static_cast<int>(std::max<double>(first, box.left()));
    const int x1 = static_cast<int>(std::min<double>(last, box.right() + 1));
    const bool is_covered = x0 < x1;
    if (is_covered) {
      DrawRow(source, triangle, weights, origin.x(), x0, x1,
              reinterpret_cast<QRgb*>(target->scanLine(y)));
    }
  }
}

}  // namespace

WarpedImage WarpImage(const QImage& source, const WarpGrid& grid,
                      const std::vector<QPointF>& points) {
  assert(!source.isNull());
  assert(points.size() <= static_cast<size_t>(kMaxWarpPoints));
  const QImage premultiplied =
      source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const std::vector<QPointF> rest = RestPoints(source.size(), grid);
  const bool is_matching = grid.IsOn() && points.size() == rest.size();
  if (!is_matching) {
    return {premultiplied, QPointF()};
  }
  QRectF bounds;
  for (const QPointF& point : points) {
    bounds = bounds.united(QRectF(point, QSizeF(1, 1)));
  }
  const QRect area = bounds.toAlignedRect().intersected(
      QRect(-kMaxWarpedSide / 2, -kMaxWarpedSide / 2, kMaxWarpedSide,
            kMaxWarpedSide));
  WarpedImage warped{QImage(area.size(), QImage::Format_ARGB32_Premultiplied),
                     QPointF(area.topLeft())};
  warped.image.fill(Qt::transparent);
  for (const MeshTriangle& t : WarpTriangles(grid)) {
    const auto a = static_cast<size_t>(t[0]);
    const auto b = static_cast<size_t>(t[1]);
    const auto c = static_cast<size_t>(t[2]);
    DrawTriangle(premultiplied,
                 {{points[a], points[b], points[c]},
                  {rest[a], rest[b], rest[c]}},
                 warped.origin, &warped.image);
  }
  return warped;
}

}  // namespace snapper
