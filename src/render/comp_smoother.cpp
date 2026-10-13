#include "render/comp_smoother.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "fsp/olm_smoother_info.h"
#include "render/comp_smoother_runs.h"

namespace snapper {
namespace {

// RGB distance within which a pixel counts as a Gamma Color.
constexpr double kGammaTolerance = 40.0;
// How far (each channel) a pixel may be from the Color Key and still be
// keyed: premultiplied pixels come back a step off.
constexpr int kKeyTolerance = 1;
constexpr int kMaxSide = 1 << 15;

double ValueAt(const FspValues& values, SmootherSlot slot) {
  const auto at = static_cast<size_t>(slot);
  assert(at < values.size());
  assert(std::isfinite(values[at]));
  return values[at];
}

QColor ColorAt(const FspValues& values, int slot) {
  assert(slot >= 0 && static_cast<size_t>(slot) < values.size());
  const double value =
      std::clamp(values[static_cast<size_t>(slot)], 0.0, double{0xFFFFFF});
  assert(std::isfinite(value));
  return QColor::fromRgb(static_cast<QRgb>(std::lround(value)) | 0xFF000000u);
}

// One pixel in the blend's space, premultiplied.
struct Light final {
  std::array<float, 4> channels{};
};

// How 8-bit colors go into the blend's space and back.
class LightSpace final {
 public:
  explicit LightSpace(const SmootherParams& params)
      : is_linear_(params.is_linear),
        gamma_(static_cast<float>(std::max(params.gamma_value, 0.01))) {
    assert(gamma_ > 0.0f);
    for (size_t i = 0; i < to_linear_.size(); ++i) {
      const float s = static_cast<float>(i) / 255.0f;
      const bool is_dark = s <= 0.04045f;
      const float linear =
          is_dark ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
      to_linear_[i] = is_linear_ ? linear : s;
    }
    assert(to_linear_.back() > 0.99f);
  }

  // straight is not premultiplied; gamma: with the Gamma Value.
  Light In(QRgb straight, bool is_gamma) const {
    const float alpha = static_cast<float>(qAlpha(straight)) / 255.0f;
    assert(alpha >= 0.0f && alpha <= 1.0f);
    const std::array<int, 3> rgb = {qRed(straight), qGreen(straight),
                                    qBlue(straight)};
    Light light;
    for (size_t c = 0; c < rgb.size(); ++c) {
      const float value = to_linear_[static_cast<size_t>(rgb[c])];
      const float bent = is_gamma ? std::pow(value, 1.0f / gamma_) : value;
      light.channels[c] = bent * alpha;
    }
    light.channels[3] = alpha;
    return light;
  }

  // Premultiplied 8-bit.
  QRgb Out(const Light& light, bool is_gamma) const {
    const float alpha = std::clamp(light.channels[3], 0.0f, 1.0f);
    assert(std::isfinite(alpha));
    const bool is_clear = alpha <= 0.0f;
    if (is_clear) {
      return 0;
    }
    std::array<int, 3> rgb{};
    for (size_t c = 0; c < rgb.size(); ++c) {
      const float bent = std::clamp(light.channels[c] / alpha, 0.0f, 1.0f);
      const float value = is_gamma ? std::pow(bent, gamma_) : bent;
      const float s = is_linear_ ? ToSrgb(value) : value;
      rgb[c] = static_cast<int>(std::lround(s * alpha * 255.0f));
    }
    assert(rgb[0] >= 0 && rgb[0] <= 255);
    const int a = static_cast<int>(std::lround(alpha * 255.0f));
    return qRgba(std::min(rgb[0], a), std::min(rgb[1], a),
                 std::min(rgb[2], a), a);
  }

 private:
  static float ToSrgb(float linear) {
    assert(linear >= 0.0f && linear <= 1.0f);
    const bool is_dark = linear <= 0.0031308f;
    return is_dark ? linear * 12.92f
                   : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
  }

  bool is_linear_;
  float gamma_;
  std::array<float, 256> to_linear_{};
};

bool IsKeyed(QRgb straight, const QColor& key) {
  assert(key.isValid());
  assert(kKeyTolerance >= 0);
  return qAlpha(straight) > 0 &&
         std::abs(qRed(straight) - key.red()) <= kKeyTolerance &&
         std::abs(qGreen(straight) - key.green()) <= kKeyTolerance &&
         std::abs(qBlue(straight) - key.blue()) <= kKeyTolerance;
}

// frame as straight pixels, keyed, every clear pixel 0.
CelGrid CelOf(const QImage& frame, const SmootherParams& params) {
  assert(frame.format() == QImage::Format_ARGB32_Premultiplied);
  assert(frame.width() <= kMaxSide && frame.height() <= kMaxSide);
  CelGrid cel;
  cel.width = frame.width();
  cel.height = frame.height();
  cel.pixels.reserve(static_cast<size_t>(cel.width) *
                     static_cast<size_t>(cel.height));
  for (int y = 0; y < cel.height; ++y) {
    const auto* row = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
    for (int x = 0; x < cel.width; ++x) {
      const QRgb straight = qUnpremultiply(row[x]);
      const bool is_clear = qAlpha(straight) == 0 ||
                            (params.uses_key && IsKeyed(straight, params.key));
      cel.pixels.push_back(is_clear ? 0u : straight);
    }
  }
  return cel;
}

bool IsGammaColor(QRgb straight, const std::vector<QColor>& colors) {
  assert(colors.size() <= static_cast<size_t>(kSmootherGammaColors));
  assert(kGammaTolerance > 0.0);
  const bool is_clear = qAlpha(straight) == 0;
  if (is_clear) {
    return false;
  }
  return std::ranges::any_of(colors, [straight](const QColor& color) {
    const double dr = qRed(straight) - color.red();
    const double dg = qGreen(straight) - color.green();
    const double db = qBlue(straight) - color.blue();
    return std::sqrt(dr * dr + dg * dg + db * db) <= kGammaTolerance;
  });
}

// The neighbour across each side; the shares only name ones inside.
std::array<size_t, 4> NeighboursOf(size_t at, int width) {
  assert(width > 0 && width <= kMaxSide);
  assert(at < static_cast<size_t>(kMaxSide) * static_cast<size_t>(kMaxSide));
  const auto step = static_cast<size_t>(width);
  // A pixel on the picture's border wraps here; its share on that side
  // is always 0, so the index is never read.
  return {at - step, at + step, at - 1, at + 1};
}

// One shared pixel: itself mixed with the neighbours it takes from.
QRgb MixPixel(const CelGrid& cel, size_t at,
              const std::array<float, 4>& shares,
              const SmootherParams& params, const LightSpace& space) {
  assert(at < cel.pixels.size());
  assert(params.smoothness > 0.0);
  const std::array<size_t, 4> around = NeighboursOf(at, cel.width);
  const QRgb self = cel.pixels[at];
  float total = 0.0f;
  bool touches_gamma = IsGammaColor(self, params.gamma_colors);
  for (size_t side = 0; side < shares.size(); ++side) {
    const bool is_taken = shares[side] > 0.0f;
    total += shares[side];
    const bool is_gamma_side =
        is_taken &&
        IsGammaColor(cel.pixels[around[side]], params.gamma_colors);
    touches_gamma = touches_gamma || is_gamma_side;
  }
  const bool is_gamma =
      params.gamma == SmootherGamma::kAllColors ||
      (params.gamma == SmootherGamma::kGammaColors && touches_gamma);
  const float strength = static_cast<float>(params.smoothness);
  const float scale = strength / std::max(1.0f, total);
  Light mixed = space.In(self, is_gamma);
  const float kept = 1.0f - std::min(1.0f, total) * strength;
  for (float& channel : mixed.channels) {
    channel *= kept;
  }
  for (size_t side = 0; side < shares.size(); ++side) {
    const bool is_taken = shares[side] > 0.0f;
    if (!is_taken) {
      continue;
    }
    const Light other = space.In(cel.pixels[around[side]], is_gamma);
    for (size_t c = 0; c < mixed.channels.size(); ++c) {
      mixed.channels[c] += other.channels[c] * shares[side] * scale;
    }
  }
  return space.Out(mixed, is_gamma);
}

}  // namespace

SmootherParams SmootherParamsOf(const FspValues& values) {
  assert(values.size() == static_cast<size_t>(kSmootherSlotCount));
  assert(OlmSmootherInfo().slot_count == kSmootherSlotCount);
  SmootherParams params;
  params.smoothness =
      std::clamp(ValueAt(values, SmootherSlot::kSmoothness) / 100.0, 0.0, 1.0);
  params.extra =
      std::clamp(ValueAt(values, SmootherSlot::kExtraSmooth) / 100.0, 0.0, 1.0);
  params.range = static_cast<int>(std::clamp(
      std::lround(ValueAt(values, SmootherSlot::kSmoothRange)), 0L, 255L));
  params.uses_key = ValueAt(values, SmootherSlot::kUseColorKey) >= 0.5;
  params.key = ColorAt(values, static_cast<int>(SmootherSlot::kColorKey));
  params.is_linear =
      std::lround(ValueAt(values, SmootherSlot::kVersion)) != kSmootherV1;
  const long gamma = std::lround(ValueAt(values, SmootherSlot::kGamma));
  params.gamma = gamma == kSmootherGammaAll      ? SmootherGamma::kAllColors
                 : gamma == kSmootherGammaListed ? SmootherGamma::kGammaColors
                                                 : SmootherGamma::kOff;
  params.gamma_value = ValueAt(values, SmootherSlot::kGammaValue);
  const long count = std::clamp(
      std::lround(ValueAt(values, SmootherSlot::kGammaCount)), 1L,
      static_cast<long>(kSmootherGammaColors));
  for (int i = 0; i < count; ++i) {
    params.gamma_colors.push_back(
        ColorAt(values, SmootherGammaColorSlot(i)));
  }
  return params;
}

QImage SmoothCel(const QImage& frame, const SmootherParams& params) {
  assert(frame.format() == QImage::Format_ARGB32_Premultiplied);
  assert(!frame.isNull());
  const CelGrid cel = CelOf(frame, params);
  const bool smooths = params.smoothness > 0.0 && params.range < 255;
  const SmootherShares shares =
      smooths ? FindSmootherShares(
                    cel, SmootherEdgeRule{params.range, params.extra,
                                          params.line_colors})
              : SmootherShares();
  const LightSpace space(params);
  QImage out(frame.size(), QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < cel.height; ++y) {
    const auto* in = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
    auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < cel.width; ++x) {
      const size_t at = static_cast<size_t>(y) *
                            static_cast<size_t>(cel.width) +
                        static_cast<size_t>(x);
      const bool is_shared =
          smooths && std::ranges::any_of(shares[at], [](float share) {
            return share > 0.0f;
          });
      const bool is_keyed = cel.pixels[at] == 0u;
      // Untouched pixels keep their exact value: no round trip.
      row[x] = is_shared ? MixPixel(cel, at, shares[at], params, space)
               : is_keyed ? 0u
                          : in[x];
    }
  }
  return out;
}

}  // namespace snapper
