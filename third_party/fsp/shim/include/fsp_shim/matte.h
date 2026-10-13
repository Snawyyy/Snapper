// Blend and premultiply over whole pictures, for the stand-in host's
// utils and FillMatte suite.

#ifndef FSP_SHIM_MATTE_H_
#define FSP_SHIM_MATTE_H_

#include "fsp_shim/host_api.h"

namespace fsp_shim {

// dst = a + (b - a) * ratio, ratio in 16.16 fixed point. All three
// pictures must have the same size and depth.
PF_Err Blend(const PF_EffectWorld* a, const PF_EffectWorld* b,
             PF_Fixed ratio, PF_EffectWorld* dst);

// Forward: dst = src * alpha + color * (1 - alpha); back: the inverse.
// The color is color16 on a 16 bit picture when given, else color8
// (null: black).
PF_Err PremultiplyColor(const PF_EffectWorld* src, const PF_Pixel* color8,
                        const PF_Pixel16* color16, bool forward,
                        PF_EffectWorld* dst);

}  // namespace fsp_shim

#endif  // FSP_SHIM_MATTE_H_
