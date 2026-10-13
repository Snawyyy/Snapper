#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QRadialGradient>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

#include "render/comp_effects.h"

namespace snapper {
namespace {

// At most this many sparkles a frame, brightest first.
constexpr int kMaxSparkles = 80;
constexpr int kRampSize = 256;

size_t At(int x, int y, int width) {
  assert(x >= 0 && y >= 0 && width > 0);
  return static_cast<size_t>(y) * static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

QImage Clear(QSize size) {
  assert(!size.isEmpty());
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  return image;
}

QRgb Premultiplied(const QColor& color, double cover) {
  assert(cover >= 0.0 && cover <= 1.0);
  const double alpha = color.alphaF() * cover;
  return qRgba(static_cast<int>(std::lround(color.red() * alpha)),
               static_cast<int>(std::lround(color.green() * alpha)),
               static_cast<int>(std::lround(color.blue() * alpha)),
               static_cast<int>(std::lround(255.0 * alpha)));
}

// The ramp of stops as kRampSize premultiplied colors, in steps bands
// when steps is 2 or more.
std::array<QRgb, kRampSize> Ramp(const std::vector<GradientStop>& stops,
                                 int steps) {
  assert(!stops.empty());
  assert(steps >= 0);
  std::array<QRgb, kRampSize> ramp{};
  for (int i = 0; i < kRampSize; ++i) {
    double t = i / static_cast<double>(kRampSize - 1);
    const bool is_banded = steps >= 2;
    if (is_banded) {
      t = std::min(std::floor(t * steps), steps - 1.0) / (steps - 1.0);
    }
    const auto next = std::ranges::find_if(
        stops, [t](const GradientStop& stop) { return stop.at >= t; });
    QColor color;
    if (next == stops.begin()) {
      color = stops.front().color;
    } else if (next == stops.end()) {
      color = stops.back().color;
    } else {
      const GradientStop& from = *(next - 1);
      const double span = std::max(next->at - from.at, 1e-6);
      const double u = (t - from.at) / span;
      const auto lerp = [u](double a, double b) { return a + (b - a) * u; };
      color = QColor::fromRgbF(
          static_cast<float>(lerp(from.color.redF(), next->color.redF())),
          static_cast<float>(lerp(from.color.greenF(), next->color.greenF())),
          static_cast<float>(lerp(from.color.blueF(), next->color.blueF())),
          static_cast<float>(lerp(from.color.alphaF(), next->color.alphaF())));
    }
    ramp[static_cast<size_t>(i)] = Premultiplied(color, 1.0);
  }
  return ramp;
}

QColor CornerColor(const std::vector<GradientStop>& stops, size_t corner) {
  assert(corner < 4);
  const bool has = corner < stops.size();
  return has ? stops[corner].color : QColor(Qt::black);
}

QRgb FourCorner(const std::vector<GradientStop>& stops, double u, double v) {
  u = std::clamp(u, 0.0, 1.0);
  v = std::clamp(v, 0.0, 1.0);
  // A (top left), top right, B (bottom right), bottom left.
  const std::array<double, 4> weights = {(1 - u) * (1 - v), u * (1 - v),
                                         u * v, (1 - u) * v};
  std::array<double, 4> sum{};
  for (size_t corner = 0; corner < 4; ++corner) {
    const QColor color = CornerColor(stops, corner);
    const double a = color.alphaF() * weights[corner];
    sum[0] += color.redF() * a;
    sum[1] += color.greenF() * a;
    sum[2] += color.blueF() * a;
    sum[3] += a;
  }
  const auto byte = [](double value) {
    return std::clamp(static_cast<int>(std::lround(value * 255.0)), 0, 255);
  };
  return qRgba(byte(sum[0]), byte(sum[1]), byte(sum[2]), byte(sum[3]));
}

// Where along the ramp (0 to 1) point p (in point space) is.
double RampAt(const GradientPlace& place, QPointF p) {
  const QPointF axis = place.b - place.a;
  const double length2 = std::max(QPointF::dotProduct(axis, axis), 1e-6);
  const double length = std::sqrt(length2);
  const QPointF from = p - place.a;
  switch (place.shape) {
    case GradientShape::kLinear:
      return QPointF::dotProduct(from, axis) / length2;
    case GradientShape::kRadial:
      return std::hypot(from.x(), from.y()) / length;
    case GradientShape::kAngle: {
      const double turn = std::atan2(from.y(), from.x()) -
                          std::atan2(axis.y(), axis.x());
      const double full = 2.0 * std::numbers::pi;
      return std::fmod(turn + 2.0 * full, full) / full;
    }
    case GradientShape::kDiamond: {
      const double along = QPointF::dotProduct(from, axis) / length;
      const double side =
          (from.x() * -axis.y() + from.y() * axis.x()) / length;
      return (std::abs(along) + std::abs(side)) / length;
    }
    case GradientShape::kFourCorner:
    case GradientShape::kFromEdge:
      break;
  }
  return 0.0;
}

}  // namespace

QImage EdgeGlow(const Mask& shape, int width, int height, QColor color,
                double size, double falloff, bool inside) {
  assert(size > 0.0);
  assert(falloff >= 0.0 && falloff <= 1.0);
  QImage out = Clear(QSize(width, height));
  const float limit = static_cast<float>(size);
  const std::vector<float> distance =
      DistanceToEdge(shape, width, height, inside, limit);
  const double power = 0.5 + 2.5 * falloff;
  std::array<QRgb, kRampSize> ramp{};
  for (int i = 0; i < kRampSize; ++i) {
    const double near = 1.0 - i / static_cast<double>(kRampSize - 1);
    ramp[static_cast<size_t>(i)] = Premultiplied(color, std::pow(near, power));
  }
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const size_t index = At(x, y, width);
      const float d = distance[index];
      const bool is_lit = d > 0.0f && d < limit;
      if (!is_lit) {
        continue;
      }
      const int slot = static_cast<int>(d / limit * (kRampSize - 1));
      const double own = inside ? shape[index] / 255.0 : 1.0;
      const QRgb lit = ramp[static_cast<size_t>(slot)];
      row[x] = own >= 1.0 ? lit
                          : qRgba(static_cast<int>(qRed(lit) * own),
                                  static_cast<int>(qGreen(lit) * own),
                                  static_cast<int>(qBlue(lit) * own),
                                  static_cast<int>(qAlpha(lit) * own));
    }
  }
  return out;
}

QImage RimBand(const Mask& shape, int width, int height, QColor color,
               double angle, double band, double falloff) {
  assert(band > 0.0);
  assert(shape.size() ==
         static_cast<size_t>(width) * static_cast<size_t>(height));
  QImage out = Clear(QSize(width, height));
  const double radians = angle * std::numbers::pi / 180.0;
  // Toward the light.
  const double dx = std::sin(radians);
  const double dy = -std::cos(radians);
  const int samples = std::clamp(static_cast<int>(std::ceil(band)), 1, 16);
  const double power = 2.0 * falloff;
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const int own = shape[At(x, y, width)];
      const bool is_inside = own > 0;
      if (!is_inside) {
        continue;
      }
      double lit = 0.0;
      for (int k = 0; k < samples; ++k) {
        const double t = band * (k + 1) / samples;
        const int sx = static_cast<int>(std::lround(x + dx * t));
        const int sy = static_cast<int>(std::lround(y + dy * t));
        const bool is_in_picture =
            sx >= 0 && sy >= 0 && sx < width && sy < height;
        if (!is_in_picture) {
          break;
        }
        const double outside = 1.0 - shape[At(sx, sy, width)] / 255.0;
        const bool is_edge = outside > 0.5;
        if (is_edge) {
          // The nearest edge wins: deeper in, the band fades.
          const double depth = static_cast<double>(k) / samples;
          lit = outside * std::pow(1.0 - depth, power);
          break;
        }
      }
      const bool is_lit = lit > 0.0;
      if (is_lit) {
        row[x] = Premultiplied(color, lit * own / 255.0);
      }
    }
  }
  return out;
}

QImage GradientOver(const std::vector<GradientStop>& stops,
                    const GradientPlace& place, int width, int height) {
  assert(!stops.empty());
  assert(width > 0 && height > 0);
  QImage out = Clear(QSize(width, height));
  const std::array<QRgb, kRampSize> ramp = Ramp(stops, place.steps);
  const bool is_edge = place.shape == GradientShape::kFromEdge;
  if (is_edge) {
    assert(place.edge != nullptr);
    const float limit = static_cast<float>(place.edge_size);
    const std::vector<float> distance =
        DistanceToEdge(*place.edge, width, height, true, limit);
    for (int y = 0; y < height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
      for (int x = 0; x < width; ++x) {
        const double t =
            std::clamp(distance[At(x, y, width)] / limit, 0.0f, 1.0f);
        row[x] = ramp[static_cast<size_t>(t * (kRampSize - 1))];
      }
    }
    return out;
  }
  // Output pixels back to point space, stepping along each row.
  const QTransform back = place.space.inverted();
  const bool is_four = place.shape == GradientShape::kFourCorner;
  const QPointF box = place.b - place.a;
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    QPointF p = back.map(QPointF(0.5, y + 0.5));
    const QPointF step = back.map(QPointF(1.5, y + 0.5)) - p;
    for (int x = 0; x < width; ++x, p += step) {
      if (is_four) {
        const double u =
            std::abs(box.x()) > 1e-6 ? (p.x() - place.a.x()) / box.x() : 0.0;
        const double v =
            std::abs(box.y()) > 1e-6 ? (p.y() - place.a.y()) / box.y() : 0.0;
        row[x] = FourCorner(stops, u, v);
        continue;
      }
      const double t = std::clamp(RampAt(place, p), 0.0, 1.0);
      row[x] = ramp[static_cast<size_t>(t * (kRampSize - 1))];
    }
  }
  return out;
}

QImage Sparkles(const QImage& source, QColor color, double threshold,
                int streaks, double length, double rotation, double scale) {
  assert(source.format() == QImage::Format_ARGB32_Premultiplied);
  assert(streaks >= 2 && scale > 0.0);
  const int cell = std::max(6, static_cast<int>(std::lround(40.0 * scale)));
  struct Spark final {
    QPointF at;
    double bright = 0.0;
  };
  std::vector<Spark> sparks;
  for (int top = 0; top < source.height(); top += cell) {
    for (int left = 0; left < source.width(); left += cell) {
      Spark best;
      for (int y = top; y < std::min(top + cell, source.height()); ++y) {
        const auto* row =
            reinterpret_cast<const QRgb*>(source.constScanLine(y));
        for (int x = left; x < std::min(left + cell, source.width()); ++x) {
          const double bright = qGray(row[x]) / 255.0 * qAlpha(row[x]) / 255.0;
          const bool is_better = bright > best.bright;
          if (is_better) {
            best = Spark{QPointF(x + 0.5, y + 0.5), bright};
          }
        }
      }
      const bool is_spark = best.bright >= threshold && best.bright > 0.0;
      if (is_spark) {
        sparks.push_back(best);
      }
    }
  }
  std::ranges::stable_sort(sparks, [](const Spark& a, const Spark& b) {
    return a.bright > b.bright;
  });
  const size_t kept =
      std::min(sparks.size(), static_cast<size_t>(kMaxSparkles));
  QImage out = Clear(source.size());
  QPainter painter(&out);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  QColor clear = color;
  clear.setAlpha(0);
  for (size_t i = 0; i < kept; ++i) {
    const Spark& spark = sparks[i];
    const double over = (spark.bright - threshold) / std::max(1.0 - threshold, 0.05);
    const double reach = length * (0.4 + 0.6 * std::clamp(over, 0.0, 1.0));
    const double glow = std::max(1.5, reach * 0.18);
    QRadialGradient halo(spark.at, glow);
    halo.setColorAt(0.0, color);
    halo.setColorAt(1.0, clear);
    painter.setBrush(halo);
    painter.drawEllipse(spark.at, glow, glow);
    const double thick = std::max(0.8, reach * 0.03);
    for (int k = 0; k < streaks; ++k) {
      const double turn = (rotation + k * 180.0 / streaks) *
                          std::numbers::pi / 180.0;
      const QPointF way(std::sin(turn) * reach, -std::cos(turn) * reach);
      const QPointF side(-way.y() / reach * thick, way.x() / reach * thick);
      QLinearGradient fade(spark.at - way, spark.at + way);
      fade.setColorAt(0.0, clear);
      fade.setColorAt(0.5, color);
      fade.setColorAt(1.0, clear);
      painter.setBrush(fade);
      painter.drawPolygon(QPolygonF({spark.at - way, spark.at + side,
                                     spark.at + way, spark.at - side}));
    }
  }
  return out;
}

QImage SpeedLines(QSize size, int style, QPointF center, double angle,
                  int density, double thickness, double clear, QColor color,
                  int seed) {
  assert(!size.isEmpty());
  assert(density > 0 && thickness > 0.0);
  QImage out = Clear(size);
  QPainter painter(&out);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  const double reach = std::hypot(size.width(), size.height());
  const bool is_burst = style == 0;
  if (is_burst) {
    const double hole = clear * std::min(size.width(), size.height()) / 2.0;
    for (int i = 0; i < density; ++i) {
      const double turn = Noise(seed, i * 3) * 2.0 * std::numbers::pi;
      const double inner = hole * (0.75 + 0.5 * Noise(seed, i * 3 + 1));
      const double half =
          thickness * (0.4 + 1.2 * Noise(seed, i * 3 + 2)) / 2.0;
      const QPointF way(std::cos(turn), std::sin(turn));
      const QPointF side(-way.y(), way.x());
      const QPointF tip = center + way * inner;
      const QPointF end = center + way * reach;
      painter.drawPolygon(
          QPolygonF({tip, end + side * half, end - side * half}));
    }
    return out;
  }
  const double radians = angle * std::numbers::pi / 180.0;
  const QPointF way(std::sin(radians), -std::cos(radians));
  const QPointF side(-way.y(), way.x());
  const QPointF middle(size.width() / 2.0, size.height() / 2.0);
  for (int i = 0; i < density; ++i) {
    const double across = (Noise(seed, i * 4) - 0.5) * reach;
    const double along = (Noise(seed, i * 4 + 1) - 0.5) * reach;
    const double long_by = reach * (0.1 + 0.3 * Noise(seed, i * 4 + 2));
    const double half =
        thickness * (0.4 + 1.2 * Noise(seed, i * 4 + 3)) / 2.0;
    const QPointF mid = middle + side * across + way * along;
    painter.drawPolygon(QPolygonF({mid - way * long_by, mid + side * half,
                                   mid + way * long_by, mid - side * half}));
  }
  return out;
}

}  // namespace snapper
