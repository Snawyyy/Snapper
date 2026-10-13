// The rest of Outline's sources (see outline_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_outline_EffectMain
#define PluginDataEntryFunction2 fsp_outline_PluginDataEntryFunction2

namespace fsp_outline {
#include "_NFLib/fx/debug_font.cpp"
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_ChannelBlur.cpp"
#include "_NFLib/fx/NF_ChannelMinMax.cpp"
#include "_NFLib/fx/NF_Draw.cpp"
#include "_NFLib/fx/NF_DrawWorld.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "_NFLib/fx/NF_Noise.cpp"
#include "_NFLib/fx/NF_Paint.cpp"
}  // namespace fsp_outline
