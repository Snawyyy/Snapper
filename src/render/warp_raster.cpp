#include "render/warp_raster.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/warp.h"

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

QRgb Blend(QRgb a, QRgb b, double t) {
  assert(t >= 0.0 && t <= 1.0);
  const auto mix = [t](int x, int y) {
    return static_cast<int>(std::lround(x + (y - x) * t));
  };
  const QRgb out = qRgba(mix(qRed(a), qRed(b)), mix(qGreen(a), qGreen(b)),
                         mix(qBlue(a), qBlue(b)), mix(qAlpha(a), qAlpha(b)));
  assert(qAlpha(out) >= 0);
  return out;
}

// Bilinear read of a premultiplied image; outside is clear.
QRgb Sample(const QImage& image, QPointF at) {
  assert(image.format() == QImage::Format_ARGB32_Premultiplied);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const double x = at.x() - 0.5;
  const double y = at.y() - 0.5;
  const int left = static_cast<int>(std::floor(x));
  const int top = static_cast<int>(std::floor(y));
  const auto pixel = [&image](int px, int py) -> QRgb {
    const bool is_inside = px >= 0 && py >= 0 && px < image.width() &&
                           py < image.height();
    const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(
        is_inside ? py : 0));
    return is_inside ? row[px] : 0u;
  };
  const double fx = x - left;
  const double fy = y - top;
  return Blend(Blend(pixel(left, top), pixel(left + 1, top), fx),
               Blend(pixel(left, top + 1), pixel(left + 1, top + 1), fx), fy);
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
  const int x0 = box.left();
  const int x1 = box.right() + 1;
  const int y0 = box.top();
  const int y1 = box.bottom() + 1;
  for (int y = y0; y < y1; ++y) {
    auto* row = reinterpret_cast<QRgb*>(target->scanLine(y));
    for (int x = x0; x < x1; ++x) {
      const QPointF p(x + 0.5 + origin.x(), y + 0.5 + origin.y());
      const double u = ((b.x() - p.x()) * (c.y() - p.y()) -
                        (c.x() - p.x()) * (b.y() - p.y())) / area;
      const double v = ((c.x() - p.x()) * (a.y() - p.y()) -
                        (a.x() - p.x()) * (c.y() - p.y())) / area;
      const double w = 1.0 - u - v;
      const bool is_inside = u >= -kEdgeSlack && v >= -kEdgeSlack &&
                             w >= -kEdgeSlack;
      if (is_inside) {
        row[x] = Sample(source, triangle.from[0] * u + triangle.from[1] * v +
                                    triangle.from[2] * w);
      }
    }
  }
}

}  // namespace

WarpedImage WarpImage(const QImage& source, WarpGrid grid,
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
  const int width = grid.columns + 1;
  for (int row = 0; row < grid.rows; ++row) {
    for (int column = 0; column < grid.columns; ++column) {
      const auto at = [width, row, column](int dr, int dc) {
        return static_cast<size_t>((row + dr) * width + column + dc);
      };
      const size_t tl = at(0, 0), tr = at(0, 1), bl = at(1, 0), br = at(1, 1);
      DrawTriangle(premultiplied,
                   {{points[tl], points[tr], points[br]},
                    {rest[tl], rest[tr], rest[br]}},
                   warped.origin, &warped.image);
      DrawTriangle(premultiplied,
                   {{points[tl], points[br], points[bl]},
                    {rest[tl], rest[br], rest[bl]}},
                   warped.origin, &warped.image);
    }
  }
  return warped;
}

}  // namespace snapper
