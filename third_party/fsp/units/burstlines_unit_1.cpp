// BurstLines in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. NF_UnMult.cpp and
// NF_VectorMask.cpp define the same file-local helpers as NF_Mult.cpp
// and NF_Blend.cpp, so they sit in burstlines_unit_2.cpp and _3.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_burstlines_EffectMain
#define PluginDataEntryFunction2 fsp_burstlines_PluginDataEntryFunction2

namespace fsp_burstlines {
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_DrawAALine.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "_NFLib/vector/NF_VectorLine.cpp"
#include "BurstLines/BrustLinesMain.cpp"
#include "BurstLines/BurstLines.cpp"
}  // namespace fsp_burstlines
