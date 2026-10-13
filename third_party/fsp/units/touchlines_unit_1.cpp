// TouchLines in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. A_Noise.cpp,
// NF_Blend.cpp and TouchLineFilter.cpp define the same file-local
// helpers as ThresholdLine.cpp, so they sit in touchlines_unit_2.cpp,
// _3.cpp and _4.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_touchlines_EffectMain
#define PluginDataEntryFunction2 fsp_touchlines_PluginDataEntryFunction2

namespace fsp_touchlines {
#include "_NFLib/fx/debug_font.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_DrawAALine.cpp"
#include "_NFLib/fx/NF_DrawWorld.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "_NFLib/fx/NF_Noise.cpp"
#include "_NFLib/fx/NF_Paint.cpp"
#include "TouchLines/ThresholdLine.cpp"
#include "TouchLines/TouchCtLine.cpp"
#include "TouchLines/TouchLines.cpp"
#include "TouchLines/TouchStLine.cpp"
}  // namespace fsp_touchlines
