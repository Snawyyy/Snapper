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

// The pixel at x, y of a premultiplied image; outside is clear.
[[gnu::always_inline]] inline QRgb PixelAt(const QImage& image, int x, int y) {
  const bool is_inside =
      x >= 0 && y >= 0 && x < image.width() && y < image.height();
  return is_inside
             ? reinterpret_cast<const QRgb*>(image.constScanLine(y))[x]
             : 0u;
}

// One channel (at shift) of four pixels mixed by weights out of 65536.
[[gnu::always_inline]] inline QRgb MixChannel(const QRgb (&p)[4],
                                              const unsigned (&w)[4],
                                              int shift) {
  const unsigned sum = ((p[0] >> shift) & 0xffu) * w[0] +
                       ((p[1] >> shift) & 0xffu) * w[1] +
                       ((p[2] >> shift) & 0xffu) * w[2] +
                       ((p[3] >> shift) & 0xffu) * w[3];
  return ((sum + 32768u) >> 16) << shift;
}

// Bilinear read of a premultiplied image: the four pixels around at,
// mixed by how near each centre is. A pixel centre reads its own pixel,
// so a drawing at rest sits where the nearest read put it.
[[gnu::always_inline]] inline QRgb Blend(const QImage& image, QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const double sx = at.x() - 0.5;
  const double sy = at.y() - 0.5;
  const double fx0 = std::floor(sx);
  const double fy0 = std::floor(sy);
  const int x = static_cast<int>(fx0);
  const int y = static_cast<int>(fy0);
  const auto ix = static_cast<unsigned>((sx - fx0) * 256.0);
  const auto iy = static_cast<unsigned>((sy - fy0) * 256.0);
  const QRgb p[4] = {PixelAt(image, x, y), PixelAt(image, x + 1, y),
                     PixelAt(image, x, y + 1), PixelAt(image, x + 1, y + 1)};
  const unsigned w[4] = {(256u - ix) * (256u - iy), ix * (256u - iy),
                         (256u - ix) * iy, ix * iy};
  return MixChannel(p, w, 0) | MixChannel(p, w, 8) | MixChannel(p, w, 16) |
         MixChannel(p, w, 24);
}

// A read as look asks.
[[gnu::always_inline]] inline QRgb ReadLooked(const QImage& image, QPointF at,
                                              const WarpLook& look) {
  const QRgb mixed = Blend(image, at);
  const int alpha = qAlpha(mixed);
  const bool is_inside = alpha == 255;
  if (is_inside) {
    return look.is_flat ? Sample(image, at) : mixed;
  }
  if (!look.is_hard) {
    return mixed;
  }
  const bool is_dropped = alpha < 128;
  if (is_dropped) {
    return 0u;
  }
  const QRgb nearest = Sample(image, at);
  const bool is_own = look.is_flat && qAlpha(nearest) == 255;
  return is_own ? nearest : (qUnpremultiply(mixed) | 0xff000000u);
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

// Draws one row of the triangle between columns x0 and x1, reading the
// source with read. Inside a triangle the source position moves the
// same step for every pixel, so it is found once and stepped along.
template <typename Read>
void DrawRow(const Read& read, const Triangle& triangle,
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
    row[x] = read(QPointF(at_x.slope * px + at_x.base,
                          at_y.slope * px + at_y.base));
  }
}

template <typename Read>
void DrawTriangle(const Read& read, const Triangle& triangle, QPointF origin,
                  QImage* target) {
  assert(target != nullptr);
  assert(!target->isNull());
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
      DrawRow(read, triangle, weights, origin.x(), x0, x1,
              reinterpret_cast<QRgb*>(target->scanLine(y)));
    }
  }
}

// Every triangle of grid, from rest to points, read with read.
template <typename Read>
void DrawTriangles(const Read& read, const WarpGrid& grid,
                   const std::vector<QPointF>& rest,
                   const std::vector<QPointF>& points, WarpedImage* warped) {
  assert(warped != nullptr);
  assert(rest.size() == points.size());
  for (const MeshTriangle& t : WarpTriangles(grid)) {
    const auto a = static_cast<size_t>(t[0]);
    const auto b = static_cast<size_t>(t[1]);
    const auto c = static_cast<size_t>(t[2]);
    DrawTriangle(read,
                 {{points[a], points[b], points[c]},
                  {rest[a], rest[b], rest[c]}},
                 warped->origin, &warped->image);
  }
}

}  // namespace

WarpLook WarpLookFor(DrawingLook look, bool hard_edges) {
  assert(static_cast<int>(look) < kDrawingLookCount);
  assert(static_cast<int>(look) >= 0);
  const bool is_crisp = look == DrawingLook::kCrisp;
  if (is_crisp) {
    return {};
  }
  return {true, look == DrawingLook::kSmoothFlat, hard_edges};
}

QRectF FrameShare(QPoint offset, QSize frame_size, QSize image_size) {
  assert(frame_size.width() >= 0 && frame_size.height() >= 0);
  assert(image_size.width() >= 0 && image_size.height() >= 0);
  const bool is_frame_sized =
      offset.isNull() && frame_size == image_size;
  if (is_frame_sized || image_size.isEmpty()) {
    return QRectF(0.0, 0.0, 1.0, 1.0);
  }
  const double wide = image_size.width();
  const double tall = image_size.height();
  return QRectF(offset.x() / wide, offset.y() / tall,
                frame_size.width() / wide, frame_size.height() / tall);
}

namespace {

// Lays one more point past each end of every line (lines of count
// points, along x or y) of the row by row grids rest and points: the
// rest point at the source's edge (low or high), its bent point carried
// on as the line's end cell bends.
void GrowLines(std::vector<QPointF>* rest, std::vector<QPointF>* points,
               int count, int lines, bool is_x, double low, double high) {
  assert(rest != nullptr && points != nullptr);
  assert(count >= 2 && lines >= 1);
  const auto along = [is_x](QPointF p) { return is_x ? p.x() : p.y(); };
  const auto at = [&](int line, int i) {
    return is_x ? static_cast<size_t>(line * count + i)
                : static_cast<size_t>(i * lines + line);
  };
  std::vector<QPointF> new_rest(static_cast<size_t>((count + 2) * lines));
  std::vector<QPointF> new_points(new_rest.size());
  const auto put = [&](int line, int i, QPointF r, QPointF p) {
    const size_t k = is_x ? static_cast<size_t>(line * (count + 2) + i)
                          : static_cast<size_t>(i * lines + line);
    new_rest[k] = r;
    new_points[k] = p;
  };
  for (int line = 0; line < lines; ++line) {
    for (int i = 0; i < count; ++i) {
      put(line, i + 1, (*rest)[at(line, i)], (*points)[at(line, i)]);
    }
    for (const bool is_low : {true, false}) {
      const int edge = is_low ? 0 : count - 1;
      const int inner = is_low ? 1 : count - 2;
      const QPointF edge_rest = (*rest)[at(line, edge)];
      const QPointF inner_rest = (*rest)[at(line, inner)];
      const double cell = along(edge_rest) - along(inner_rest);
      const double reach = (is_low ? low : high) - along(edge_rest);
      const double share = cell != 0.0 ? reach / cell : 0.0;
      QPointF far_rest = edge_rest;
      (is_x ? far_rest.rx() : far_rest.ry()) = is_low ? low : high;
      const QPointF edge_point = (*points)[at(line, edge)];
      const QPointF far_point =
          edge_point + (edge_point - (*points)[at(line, inner)]) * share;
      put(line, is_low ? 0 : count + 1, far_rest, far_point);
    }
  }
  *rest = std::move(new_rest);
  *points = std::move(new_points);
}

}  // namespace

WarpedImage WarpImage(const QImage& source, const WarpGrid& grid,
                      const std::vector<QPointF>& points,
                      const WarpLook& look) {
  assert(!source.isNull());
  assert(points.size() <= static_cast<size_t>(kMaxWarpPoints));
  const QImage premultiplied =
      source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  // The frame in source pixels; the grid rests over it, not the source.
  const QRectF frame(look.frame.x() * source.width(),
                     look.frame.y() * source.height(),
                     look.frame.width() * source.width(),
                     look.frame.height() * source.height());
  std::vector<QPointF> rest =
      RestPoints(frame.size().toSize(), grid);
  for (QPointF& point : rest) {
    point += frame.topLeft();
  }
  const bool is_matching = grid.IsOn() && points.size() == rest.size();
  if (!is_matching) {
    return {premultiplied, -frame.topLeft()};
  }
  // A drawing past its frame: the grid grows a ring of cells out to the
  // source's edges, bending as its edge cells do, so the overhang comes
  // along. (A shape mesh keeps to its own outline.)
  const bool has_overhang = look.frame != QRectF(0.0, 0.0, 1.0, 1.0);
  WarpGrid used = grid;
  std::vector<QPointF> targets = points;
  const bool can_grow = has_overhang && !grid.IsMesh() &&
                        grid.columns >= 1 && grid.rows >= 1;
  if (can_grow) {
    GrowLines(&rest, &targets, grid.columns + 1, grid.rows + 1, true, 0.0,
              source.width());
    GrowLines(&rest, &targets, grid.rows + 1, grid.columns + 3, false, 0.0,
              source.height());
    used = WarpGrid{grid.columns + 2, grid.rows + 2, nullptr};
  }
  QRectF bounds;
  for (const QPointF& point : targets) {
    bounds = bounds.united(QRectF(point, QSizeF(1, 1)));
  }
  const QRect area = bounds.toAlignedRect().intersected(
      QRect(-kMaxWarpedSide / 2, -kMaxWarpedSide / 2, kMaxWarpedSide,
            kMaxWarpedSide));
  WarpedImage warped{QImage(area.size(), QImage::Format_ARGB32_Premultiplied),
                     QPointF(area.topLeft())};
  warped.image.fill(Qt::transparent);
  if (look.is_blended) {
    DrawTriangles(
        [&](QPointF at) { return ReadLooked(premultiplied, at, look); }, used,
        rest, targets, &warped);
  } else {
    DrawTriangles([&](QPointF at) { return Sample(premultiplied, at); }, used,
                  rest, targets, &warped);
  }
  return warped;
}

}  // namespace snapper
