// Star, part 1 of 2, in a namespace of its own so its helpers never meet
// another plugin's at link time. Extract.cpp and StarBlend.cpp both
// define the inline GetMaxChannel (the same body), so StarBlend.cpp has
// its own unit. The entry points have C linkage, which ignores
// namespaces: they are renamed instead. DrawStar.cpp is vendored but not
// compiled: F's project leaves it out.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_star_EffectMain
#define PluginDataEntryFunction2 fsp_star_PluginDataEntryFunction2

namespace fsp_star {
#include "_NFLib/fx/NF_Mult.cpp"
#include "Star/Extract.cpp"
#include "Star/Star.cpp"
#include "Star/StarMain.cpp"
}  // namespace fsp_star
