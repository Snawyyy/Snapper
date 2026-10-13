// SpeedLines in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. A_Noise.cpp defines the
// same file-local helper as NF_Blend.cpp, so it sits in
// speedlines_unit_2.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_speedlines_EffectMain
#define PluginDataEntryFunction2 fsp_speedlines_PluginDataEntryFunction2

namespace fsp_speedlines {
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_DrawAALine.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "SpeedLines/SpeedLines.cpp"
#include "SpeedLines/SpeedLinesFilter.cpp"
}  // namespace fsp_speedlines
