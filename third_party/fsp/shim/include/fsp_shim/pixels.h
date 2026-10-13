// Pixel access and depth conversion shared by the stand-in host's
// picture code.

#ifndef FSP_SHIM_PIXELS_H_
#define FSP_SHIM_PIXELS_H_

#include "fsp_shim/ae_types.h"

namespace fsp_shim {

template <typename Pixel>
Pixel* PixelAt(const PF_EffectWorld* world, A_long x, A_long y) {
  unsigned char* row = reinterpret_cast<unsigned char*>(world->data) +
                       static_cast<std::ptrdiff_t>(y) * world->rowbytes;
  return reinterpret_cast<Pixel*>(row) + x;
}

// The largest channel value of each depth (After Effects' 16 bit tops
// out at 32768).
template <typename Pixel>
constexpr double ChannelMax() {
  return 1.0;
}
template <>
constexpr double ChannelMax<PF_Pixel>() {
  return PF_MAX_CHAN8;
}
template <>
constexpr double ChannelMax<PF_Pixel16>() {
  return PF_MAX_CHAN16;
}

inline A_u_short To16(A_u_char value) {
  return static_cast<A_u_short>((value * PF_MAX_CHAN16 + PF_HALF_CHAN8) /
                                PF_MAX_CHAN8);
}

inline PF_Pixel16 To16(const PF_Pixel& pixel) {
  return PF_Pixel16{To16(pixel.alpha), To16(pixel.red), To16(pixel.green),
                    To16(pixel.blue)};
}

inline PF_PixelFloat ToFloat(const PF_Pixel& pixel) {
  const float scale = 1.0f / PF_MAX_CHAN8;
  return PF_PixelFloat{pixel.alpha * scale, pixel.red * scale,
                       pixel.green * scale, pixel.blue * scale};
}

}  // namespace fsp_shim

#endif  // FSP_SHIM_PIXELS_H_
