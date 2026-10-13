#include "render/comp_effects.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

namespace snapper {
namespace {

constexpr int kMaxSmearSamples = 16;

size_t At(int x, int y, int width) {
  assert(x >= 0 && y >= 0 && width > 0);
  return static_cast<size_t>(y) * static_cast<size_t>(width) +
         static_cast<size_t>(x);
}

// Distance in RGB from pixel (premultiplied) to color, unpremultiplied.
double ColorDistance(QRgb pixel, const QColor& color) {
  const int alpha = qAlpha(pixel);
  const bool is_clear = alpha == 0;
  if (is_clear) {
    return 1000.0;
  }
  const double unmul = 255.0 / alpha;
  const double dr = qRed(pixel) * unmul - color.red();
  const double dg = qGreen(pixel) * unmul - color.green();
  const double db = qBlue(pixel) * unmul - color.blue();
  return std::sqrt(dr * dr + dg * dg + db * db);
}

// How much a pixel distance from a color counts as that color.
double KeyWeight(double distance, double tolerance, double feather) {
  assert(tolerance >= 0.0 && feather >= 0.0);
  const bool is_in = distance <= tolerance;
  if (is_in) {
    return 1.0;
  }
  const double fade = std::max(1.0, tolerance * feather * 2.0);
  return std::clamp(1.0 - (distance - tolerance) / fade, 0.0, 1.0);
}

QImage Clear(QSize size) {
  assert(!size.isEmpty());
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  return image;
}

}  // namespace

double Noise(int seed, int index) {
  std::uint32_t h = static_cast<std::uint32_t>(seed) * 0x9E3779B1u ^
                    static_cast<std::uint32_t>(index) * 0x85EBCA77u;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0x1000000);
}

Mask AlphaOf(const QImage& picture) {
  assert(picture.format() == QImage::Format_ARGB32_Premultiplied);
  assert(!picture.isNull());
  const int width = picture.width();
  Mask mask(static_cast<size_t>(width) *
            static_cast<size_t>(picture.height()));
  for (int y = 0; y < picture.height(); ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(picture.constScanLine(y));
    for (int x = 0; x < width; ++x) {
      mask[At(x, y, width)] = static_cast<std::uint8_t>(qAlpha(row[x]));
    }
  }
  return mask;
}

Mask ColorMaskOf(const QImage& picture, const std::vector<QColor>& colors,
                 double tolerance, double feather) {
  assert(picture.format() == QImage::Format_ARGB32_Premultiplied);
  assert(!colors.empty());
  const int width = picture.width();
  Mask mask(static_cast<size_t>(width) *
            static_cast<size_t>(picture.height()));
  for (int y = 0; y < picture.height(); ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(picture.constScanLine(y));
    QRgb last = 0;
    std::uint8_t last_value = 0;
    for (int x = 0; x < width; ++x) {
      // Flat cels repeat a color along a row: reuse the last answer.
      const bool is_repeat = x > 0 && row[x] == last;
      if (is_repeat) {
        mask[At(x, y, width)] = last_value;
        continue;
      }
      double weight = 0.0;
      for (const QColor& color : colors) {
        weight = std::max(weight, KeyWeight(ColorDistance(row[x], color),
                                            tolerance, feather));
      }
      last = row[x];
      last_value = static_cast<std::uint8_t>(std::lround(weight * 255.0));
      mask[At(x, y, width)] = last_value;
    }
  }
  return mask;
}

Mask Both(const Mask& a, const Mask& b) {
  assert(a.size() == b.size());
  assert(!a.empty());
  Mask out(a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    out[i] = static_cast<std::uint8_t>((a[i] * b[i] + 127) / 255);
  }
  return out;
}

std::vector<float> DistanceToEdge(const Mask& shape, int width, int height,
                                  bool inside, float limit) {
  assert(shape.size() ==
         static_cast<size_t>(width) * static_cast<size_t>(height));
  assert(limit > 0.0f);
  constexpr float kStraight = 1.0f;
  constexpr float kDiagonal = std::numbers::sqrt2_v<float>;
  const float far = limit + 2.0f;
  std::vector<float> distance(shape.size(), far);
  for (size_t i = 0; i < shape.size(); ++i) {
    const bool is_in = shape[i] >= 128;
    const bool is_source = inside ? !is_in : is_in;
    if (is_source) {
      distance[i] = 0.0f;
    }
  }
  const auto relax = [&](int x, int y, int dx, int dy, float step) {
    const int nx = x + dx;
    const int ny = y + dy;
    const bool is_outside_picture =
        nx < 0 || ny < 0 || nx >= width || ny >= height;
    if (is_outside_picture) {
      return;
    }
    float& here = distance[At(x, y, width)];
    here = std::min(here, distance[At(nx, ny, width)] + step);
  };
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      relax(x, y, -1, 0, kStraight);
      relax(x, y, 0, -1, kStraight);
      relax(x, y, -1, -1, kDiagonal);
      relax(x, y, 1, -1, kDiagonal);
    }
  }
  for (int y = height - 1; y >= 0; --y) {
    for (int x = width - 1; x >= 0; --x) {
      relax(x, y, 1, 0, kStraight);
      relax(x, y, 0, 1, kStraight);
      relax(x, y, 1, 1, kDiagonal);
      relax(x, y, -1, 1, kDiagonal);
    }
  }
  return distance;
}

QImage SwapColors(const QImage& frame, const std::vector<ColorSwap>& swaps,
                  double tolerance, double feather) {
  assert(frame.format() == QImage::Format_ARGB32_Premultiplied);
  assert(tolerance >= 0.0);
  QImage out = Clear(frame.size());
  const bool has_swaps = !swaps.empty();
  if (!has_swaps) {
    return out;
  }
  for (int y = 0; y < frame.height(); ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
    auto* target = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < frame.width(); ++x) {
      const int alpha = qAlpha(row[x]);
      const bool is_clear = alpha == 0;
      if (is_clear) {
        continue;
      }
      double best = 0.0;
      const ColorSwap* chosen = nullptr;
      for (const ColorSwap& swap : swaps) {
        const double weight =
            KeyWeight(ColorDistance(row[x], swap.from), tolerance, feather);
        const bool is_better = weight > best;
        if (is_better) {
          best = weight;
          chosen = &swap;
        }
      }
      const bool is_swapped = chosen != nullptr;
      if (is_swapped) {
        const double cover = best * alpha / 255.0;
        const QColor& to = chosen->to;
        target[x] = qRgba(static_cast<int>(std::lround(to.red() * cover)),
                          static_cast<int>(std::lround(to.green() * cover)),
                          static_cast<int>(std::lround(to.blue() * cover)),
                          static_cast<int>(std::lround(255.0 * cover)));
      }
    }
  }
  return out;
}

QImage SmearAlong(const QImage& source, double angle, double length,
                  double streaks, int seed) {
  assert(source.format() == QImage::Format_ARGB32_Premultiplied);
  assert(length >= 0.0);
  const bool is_still = length < 1.0;
  if (is_still) {
    return source.copy();
  }
  const int width = source.width();
  const int height = source.height();
  const double radians = angle * std::numbers::pi / 180.0;
  // Trails lie behind the move: each pixel reads ahead of itself.
  const QPointF way(std::sin(radians), -std::cos(radians));
  const QPointF across(-way.y(), way.x());
  const int samples =
      std::clamp(static_cast<int>(std::lround(length)), 2, kMaxSmearSamples);
  // Streak lengths come in a few steps, each with its sample offsets
  // worked out once.
  constexpr int kReaches = 8;
  std::array<std::array<QPoint, kMaxSmearSamples>, kReaches> offsets{};
  for (int level = 0; level < kReaches; ++level) {
    const double reach =
        length * (1.0 - streaks * level / static_cast<double>(kReaches - 1));
    for (int k = 0; k < samples; ++k) {
      const double t = reach * k / (samples - 1);
      offsets[static_cast<size_t>(level)][static_cast<size_t>(k)] =
          QPoint(static_cast<int>(std::lround(way.x() * t)),
                 static_cast<int>(std::lround(way.y() * t)));
    }
  }
  // Only pixels that can reach something drawn are worked on.
  int left = width;
  int right = -1;
  int top = height;
  int bottom = -1;
  for (int y = 0; y < height; ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(source.constScanLine(y));
    for (int x = 0; x < width; ++x) {
      const bool is_drawn = qAlpha(row[x]) > 0;
      if (is_drawn) {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
      }
    }
  }
  QImage out = Clear(source.size());
  const bool is_empty = right < 0;
  if (is_empty) {
    return out;
  }
  const QRect drawn(QPoint(left, top), QPoint(right, bottom));
  const QPoint back(static_cast<int>(std::lround(-way.x() * length)),
                    static_cast<int>(std::lround(-way.y() * length)));
  const QRect reached =
      (drawn | drawn.translated(back)).adjusted(-1, -1, 1, 1) &
      QRect(0, 0, width, height);
  for (int y = reached.top(); y <= reached.bottom(); ++y) {
    auto* target = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = reached.left(); x <= reached.right(); ++x) {
      // Streaks: each thin strip along the move reaches its own length.
      const int strip = static_cast<int>(
          std::floor((x * across.x() + y * across.y()) / 2.0));
      const int level = static_cast<int>(Noise(seed, strip & 0xFFFF) *
                                         (kReaches - 1) + 0.5);
      const auto& steps = offsets[static_cast<size_t>(level)];
      std::array<int, 4> sum{};
      for (int k = 0; k < samples; ++k) {
        const int sx = x + steps[static_cast<size_t>(k)].x();
        const int sy = y + steps[static_cast<size_t>(k)].y();
        const bool is_inside =
            sx >= 0 && sy >= 0 && sx < width && sy < height;
        if (!is_inside) {
          continue;
        }
        const QRgb pixel = reinterpret_cast<const QRgb*>(
            source.constScanLine(sy))[sx];
        sum[0] += qRed(pixel);
        sum[1] += qGreen(pixel);
        sum[2] += qBlue(pixel);
        sum[3] += qAlpha(pixel);
      }
      const auto mean = [samples](int total) {
        return std::clamp(total / samples, 0, 255);
      };
      target[x] = qRgba(mean(sum[0]), mean(sum[1]), mean(sum[2]),
                        mean(sum[3]));
    }
  }
  return out;
}

QImage Finish(const QImage& frame, int style, double size, int levels,
              int seed) {
  assert(frame.format() == QImage::Format_ARGB32_Premultiplied);
  assert(size > 0.0 && levels >= 2);
  QImage out = frame.copy();
  const int cell = std::max(1, static_cast<int>(std::lround(size)));
  const int step = 255 / (levels - 1);
  for (int y = 0; y < out.height(); ++y) {
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    const bool is_dark_row = (y / cell) % 2 == 1;
    for (int x = 0; x < out.width(); ++x) {
      const QRgb pixel = row[x];
      const int alpha = qAlpha(pixel);
      const auto keep = [alpha](double value) {
        return std::clamp(static_cast<int>(std::lround(value)), 0, alpha);
      };
      switch (style) {
        case 0: {
          const int index = (y / cell) * 4099 + x / cell;
          const double grain = (Noise(seed, index) - 0.5) * 0.35 * alpha;
          row[x] = qRgba(keep(qRed(pixel) + grain), keep(qGreen(pixel) + grain),
                         keep(qBlue(pixel) + grain), alpha);
          break;
        }
        case 1:
          if (is_dark_row) {
            row[x] = qRgba(qRed(pixel) / 2, qGreen(pixel) / 2,
                           qBlue(pixel) / 2, alpha);
          }
          break;
        default: {
          const auto snap = [step, alpha](int value) {
            const int stepped = (value + step / 2) / step * step;
            return std::min(stepped, alpha);
          };
          row[x] = qRgba(snap(qRed(pixel)), snap(qGreen(pixel)),
                         snap(qBlue(pixel)), alpha);
          break;
        }
      }
    }
  }
  return out;
}

}  // namespace snapper
