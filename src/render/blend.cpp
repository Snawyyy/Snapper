#include "render/blend.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

using Rgb = std::array<float, 3>;

// picture where it differs from frame, clear where it is the same.
QImage ChangedOnly(const QImage& picture, const QImage& frame) {
  assert(picture.size() == frame.size());
  assert(picture.format() == frame.format());
  QImage changed = picture.copy();
  for (int y = 0; y < changed.height(); ++y) {
    const auto* below = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
    auto* row = reinterpret_cast<QRgb*>(changed.scanLine(y));
    for (int x = 0; x < changed.width(); ++x) {
      const bool is_same = row[x] == below[x];
      row[x] = is_same ? 0u : row[x];
    }
  }
  return changed;
}

float HardLight(float below, float top) {
  const bool is_dark = top <= 0.5f;
  if (is_dark) {
    return below * 2.0f * top;
  }
  const float lifted = 2.0f * top - 1.0f;
  return below + lifted - below * lifted;
}

float SoftLight(float below, float top) {
  const bool is_dark = top <= 0.5f;
  if (is_dark) {
    return below - (1.0f - 2.0f * top) * below * (1.0f - below);
  }
  const float d = below <= 0.25f
                      ? ((16.0f * below - 12.0f) * below + 4.0f) * below
                      : std::sqrt(below);
  return below + (2.0f * top - 1.0f) * (d - below);
}

// One channel of a separable mode: below and top unpremultiplied.
float Separable(BlendMode mode, float below, float top) {
  switch (mode) {
    case BlendMode::kNormal:
      return top;
    case BlendMode::kDarken:
      return std::min(below, top);
    case BlendMode::kMultiply:
      return below * top;
    case BlendMode::kColorBurn:
      return below >= 1.0f ? 1.0f
             : top <= 0.0f ? 0.0f
                           : 1.0f - std::min(1.0f, (1.0f - below) / top);
    case BlendMode::kLinearBurn:
      return std::max(0.0f, below + top - 1.0f);
    case BlendMode::kLighten:
      return std::max(below, top);
    case BlendMode::kScreen:
      return below + top - below * top;
    case BlendMode::kColorDodge:
      return below <= 0.0f ? 0.0f
             : top >= 1.0f ? 1.0f
                           : std::min(1.0f, below / (1.0f - top));
    case BlendMode::kAdd:
      return std::min(1.0f, below + top);
    case BlendMode::kOverlay:
      return HardLight(top, below);
    case BlendMode::kSoftLight:
      return SoftLight(below, top);
    case BlendMode::kHardLight:
      return HardLight(below, top);
    case BlendMode::kDifference:
      return std::abs(below - top);
    case BlendMode::kExclusion:
      return below + top - 2.0f * below * top;
    case BlendMode::kSubtract:
      return std::max(0.0f, below - top);
    case BlendMode::kHue:
    case BlendMode::kSaturation:
    case BlendMode::kColor:
    case BlendMode::kLuminosity:
      break;
  }
  return top;
}

float Lum(const Rgb& c) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

Rgb ClipColor(Rgb c) {
  const float l = Lum(c);
  const float low = std::min({c[0], c[1], c[2]});
  const float high = std::max({c[0], c[1], c[2]});
  for (float& v : c) {
    const bool is_under = low < 0.0f && l - low > 1e-6f;
    if (is_under) {
      v = l + (v - l) * l / (l - low);
    }
    const bool is_over = high > 1.0f && high - l > 1e-6f;
    if (is_over) {
      v = l + (v - l) * (1.0f - l) / (high - l);
    }
  }
  return c;
}

Rgb SetLum(Rgb c, float l) {
  const float d = l - Lum(c);
  for (float& v : c) {
    v += d;
  }
  return ClipColor(c);
}

float Sat(const Rgb& c) {
  return std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]});
}

Rgb SetSat(Rgb c, float s) {
  const float low = std::min({c[0], c[1], c[2]});
  const float high = std::max({c[0], c[1], c[2]});
  const float span = high - low;
  for (float& v : c) {
    v = span > 1e-6f ? (v - low) * s / span : 0.0f;
  }
  return c;
}

Rgb NonSeparable(BlendMode mode, const Rgb& below, const Rgb& top) {
  switch (mode) {
    case BlendMode::kHue:
      return SetLum(SetSat(top, Sat(below)), Lum(below));
    case BlendMode::kSaturation:
      return SetLum(SetSat(below, Sat(top)), Lum(below));
    case BlendMode::kColor:
      return SetLum(top, Lum(below));
    default:
      return SetLum(below, Lum(top));
  }
}

bool IsNonSeparable(BlendMode mode) {
  return mode == BlendMode::kHue || mode == BlendMode::kSaturation ||
         mode == BlendMode::kColor || mode == BlendMode::kLuminosity;
}

}  // namespace

void BlendOnto(const QImage& top, BlendMode mode, double opacity,
               const std::vector<std::uint8_t>& mask, QImage* frame) {
  assert(frame != nullptr && frame->size() == top.size());
  assert(top.format() == QImage::Format_ARGB32_Premultiplied &&
         frame->format() == QImage::Format_ARGB32_Premultiplied);
  const bool has_mask = !mask.empty();
  assert(!has_mask || mask.size() == static_cast<size_t>(top.width()) *
                                         static_cast<size_t>(top.height()));
  const float strength = static_cast<float>(std::clamp(opacity, 0.0, 1.0));
  const bool is_off = strength <= 0.0f;
  if (is_off) {
    return;
  }
  const bool is_whole = IsNonSeparable(mode);
  const int width = top.width();
  for (int y = 0; y < top.height(); ++y) {
    const auto* in = reinterpret_cast<const QRgb*>(top.constScanLine(y));
    auto* out = reinterpret_cast<QRgb*>(frame->scanLine(y));
    const std::uint8_t* cover =
        has_mask ? mask.data() + static_cast<size_t>(y) *
                                     static_cast<size_t>(width)
                 : nullptr;
    for (int x = 0; x < width; ++x) {
      const QRgb s = in[x];
      const float coverage =
          cover != nullptr ? static_cast<float>(cover[x]) / 255.0f : 1.0f;
      const float as = static_cast<float>(qAlpha(s)) / 255.0f * strength *
                       coverage;
      const bool is_clear = as <= 0.0f;
      if (is_clear) {
        continue;
      }
      const QRgb b = out[x];
      const float ab = static_cast<float>(qAlpha(b)) / 255.0f;
      const float raw_as = static_cast<float>(qAlpha(s)) / 255.0f;
      // Unpremultiplied colors.
      const Rgb cs = {static_cast<float>(qRed(s)) / 255.0f / raw_as,
                      static_cast<float>(qGreen(s)) / 255.0f / raw_as,
                      static_cast<float>(qBlue(s)) / 255.0f / raw_as};
      const float inv_ab = ab > 0.0f ? 1.0f / ab : 0.0f;
      const Rgb cb = {static_cast<float>(qRed(b)) / 255.0f * inv_ab,
                      static_cast<float>(qGreen(b)) / 255.0f * inv_ab,
                      static_cast<float>(qBlue(b)) / 255.0f * inv_ab};
      Rgb mixed = is_whole ? NonSeparable(mode, cb, cs) : Rgb{};
      if (!is_whole) {
        for (size_t c = 0; c < 3; ++c) {
          mixed[c] = Separable(mode, std::min(cb[c], 1.0f),
                               std::min(cs[c], 1.0f));
        }
      }
      const float ao = as + ab * (1.0f - as);
      std::array<int, 3> result{};
      for (size_t c = 0; c < 3; ++c) {
        // W3C: cs(1 - ab) + cb(1 - as) + as ab B(Cb, Cs), premultiplied.
        const float value = as * (1.0f - ab) * std::min(cs[c], 1.0f) +
                            ab * (1.0f - as) * cb[c] +
                            as * ab * std::clamp(mixed[c], 0.0f, 1.0f);
        result[c] = std::clamp(static_cast<int>(std::lround(value * 255.0f)),
                               0, 255);
      }
      const int alpha =
          std::clamp(static_cast<int>(std::lround(ao * 255.0f)), 0, 255);
      out[x] = qRgba(std::min(result[0], alpha), std::min(result[1], alpha),
                     std::min(result[2], alpha), alpha);
    }
  }
}

void StandInOnto(const QImage& picture, BlendMode mode, double opacity,
                 const std::vector<std::uint8_t>& mask, QImage* frame) {
  assert(frame != nullptr && frame->size() == picture.size());
  assert(picture.format() == QImage::Format_ARGB32_Premultiplied &&
         frame->format() == QImage::Format_ARGB32_Premultiplied);
  const bool is_normal = mode == BlendMode::kNormal;
  if (!is_normal) {
    BlendOnto(ChangedOnly(picture, *frame), mode, opacity, mask, frame);
    return;
  }
  const bool has_mask = !mask.empty();
  const float strength = static_cast<float>(std::clamp(opacity, 0.0, 1.0));
  const int width = picture.width();
  for (int y = 0; y < picture.height(); ++y) {
    const auto* in = reinterpret_cast<const QRgb*>(picture.constScanLine(y));
    auto* out = reinterpret_cast<QRgb*>(frame->scanLine(y));
    const size_t row = static_cast<size_t>(y) * static_cast<size_t>(width);
    for (int x = 0; x < width; ++x) {
      const float cover =
          has_mask ? static_cast<float>(mask[row + static_cast<size_t>(x)]) /
                         255.0f
                   : 1.0f;
      const float t = strength * cover;
      const bool is_kept = t <= 0.0f || in[x] == out[x];
      if (is_kept) {
        continue;
      }
      const auto mix = [t](int from, int to) {
        return static_cast<int>(
            std::lround(static_cast<float>(from) +
                        static_cast<float>(to - from) * t));
      };
      out[x] = qRgba(mix(qRed(out[x]), qRed(in[x])),
                     mix(qGreen(out[x]), qGreen(in[x])),
                     mix(qBlue(out[x]), qBlue(in[x])),
                     mix(qAlpha(out[x]), qAlpha(in[x])));
    }
  }
}

}  // namespace snapper
