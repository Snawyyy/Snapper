// RadialLines in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. NF_Blend.cpp and
// NF_ChannelBlur.cpp define the same file-local helpers as
// RadialLinesFilter.h and NF_Blur.cpp, so they sit in
// radiallines_unit_2.cpp and _3.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_radiallines_EffectMain
#define PluginDataEntryFunction2 fsp_radiallines_PluginDataEntryFunction2

namespace fsp_radiallines {
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_ChannelMinMax.cpp"
#include "_NFLib/fx/NF_DrawAALine.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "RadialLines/A_Noise.cpp"
#include "RadialLines/RadialLines.cpp"
#include "RadialLines/RadialLinesFilter.cpp"
}  // namespace fsp_radiallines
