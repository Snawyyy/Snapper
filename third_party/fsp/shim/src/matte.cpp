// Blend and premultiply, as After Effects' utils and FillMatte suite do
// them, in all three pixel depths.

#include <cassert>

#include "fsp_shim/matte.h"
#include "fsp_shim/pixels.h"
#include "fsp_shim/session.h"

namespace {

using fsp_shim::ChannelMax;
using fsp_shim::PixelAt;

template <typename Channel>
Channel FromDouble(double value, double top) {
  const double clamped = value < 0.0 ? 0.0 : (value > top ? top : value);
  const bool is_float = top == 1.0;
  return static_cast<Channel>(is_float ? clamped : clamped + 0.5);
}

template <typename Pixel>
void BlendWorlds(const PF_EffectWorld* a, const PF_EffectWorld* b,
                 double t, PF_EffectWorld* dst) {
  using Channel = decltype(Pixel{}.alpha);
  const double top = ChannelMax<Pixel>();
  for (A_long y = 0; y < dst->height; ++y) {
    for (A_long x = 0; x < dst->width; ++x) {
      const Pixel& p = *PixelAt<Pixel>(a, x, y);
      const Pixel& q = *PixelAt<Pixel>(b, x, y);
      Pixel* out = PixelAt<Pixel>(dst, x, y);
      out->alpha = FromDouble<Channel>(p.alpha + (q.alpha - p.alpha) * t,
                                       top);
      out->red = FromDouble<Channel>(p.red + (q.red - p.red) * t, top);
      out->green =
          FromDouble<Channel>(p.green + (q.green - p.green) * t, top);
      out->blue = FromDouble<Channel>(p.blue + (q.blue - p.blue) * t, top);
    }
  }
}

// Forward: c * a + matte * (1 - a). Back: the inverse, where a > 0.
template <typename Pixel>
void PremultiplyWorld(const PF_EffectWorld* src, const Pixel& matte,
                      bool forward, PF_EffectWorld* dst) {
  using Channel = decltype(Pixel{}.alpha);
  const double top = ChannelMax<Pixel>();
  const A_long width = src->width < dst->width ? src->width : dst->width;
  const A_long height =
      src->height < dst->height ? src->height : dst->height;
  for (A_long y = 0; y < height; ++y) {
    for (A_long x = 0; x < width; ++x) {
      Pixel pixel = *PixelAt<Pixel>(src, x, y);
      const double a = pixel.alpha / top;
      const bool can_undo = a > 0.0;
      double r = pixel.red;
      double g = pixel.green;
      double b = pixel.blue;
      if (forward) {
        r = r * a + matte.red * (1.0 - a);
        g = g * a + matte.green * (1.0 - a);
        b = b * a + matte.blue * (1.0 - a);
      } else if (can_undo) {
        r = (r - matte.red * (1.0 - a)) / a;
        g = (g - matte.green * (1.0 - a)) / a;
        b = (b - matte.blue * (1.0 - a)) / a;
      }
      pixel.red = FromDouble<Channel>(r, top);
      pixel.green = FromDouble<Channel>(g, top);
      pixel.blue = FromDouble<Channel>(b, top);
      *PixelAt<Pixel>(dst, x, y) = pixel;
    }
  }
}

bool SameShape(const PF_EffectWorld* a, const PF_EffectWorld* b) {
  return a->width == b->width && a->height == b->height &&
         fsp_shim::WorldFormat(a) == fsp_shim::WorldFormat(b) &&
         a->data != nullptr && b->data != nullptr;
}

}  // namespace

namespace fsp_shim {

PF_Err Blend(const PF_EffectWorld* a, const PF_EffectWorld* b,
             PF_Fixed ratio, PF_EffectWorld* dst) {
  const bool is_bad = a == nullptr || b == nullptr || dst == nullptr ||
                      !SameShape(a, b) || !SameShape(a, dst);
  if (is_bad) {
    return fsp_shim_fail("blend: worlds of different size or depth");
  }
  const double t = ratio / 65536.0;
  switch (WorldFormat(dst)) {
    case PF_PixelFormat_ARGB64:
      BlendWorlds<PF_Pixel16>(a, b, t, dst);
      break;
    case PF_PixelFormat_ARGB128:
      BlendWorlds<PF_PixelFloat>(a, b, t, dst);
      break;
    default:
      BlendWorlds<PF_Pixel>(a, b, t, dst);
      break;
  }
  return PF_Err_NONE;
}

PF_Err PremultiplyColor(const PF_EffectWorld* src, const PF_Pixel* color8,
                        const PF_Pixel16* color16, bool forward,
                        PF_EffectWorld* dst) {
  const bool is_bad = src == nullptr || dst == nullptr ||
                      src->data == nullptr || dst->data == nullptr ||
                      WorldFormat(src) != WorldFormat(dst);
  if (is_bad) {
    return fsp_shim_fail("premultiply: worlds of different depth");
  }
  const PF_Pixel black{0, 0, 0, 0};
  const PF_Pixel matte8 = color8 != nullptr ? *color8 : black;
  const PF_Pixel16 matte16 = color16 != nullptr ? *color16 : To16(matte8);
  switch (WorldFormat(dst)) {
    case PF_PixelFormat_ARGB64:
      PremultiplyWorld(src, matte16, forward, dst);
      break;
    case PF_PixelFormat_ARGB128:
      PremultiplyWorld(src, ToFloat(matte8), forward, dst);
      break;
    default:
      PremultiplyWorld(src, matte8, forward, dst);
      break;
  }
  return PF_Err_NONE;
}

}  // namespace fsp_shim
