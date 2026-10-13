// FloodFill as one translation unit in a namespace of its own, so its
// helpers never meet another plugin's at link time. The entry points
// have C linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_floodfill_EffectMain
#define PluginDataEntryFunction2 fsp_floodfill_PluginDataEntryFunction2

namespace fsp_floodfill {
#include "FloodFill/FloodFill.cpp"
#include "_NFLib/fx/debug_font.cpp"
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_Draw.cpp"
#include "_NFLib/fx/NF_DrawWorld.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "_NFLib/fx/NF_Noise.cpp"
#include "_NFLib/fx/NF_Paint.cpp"
}  // namespace fsp_floodfill
