// Grain, part 1 of 2, in a namespace of its own so its helpers never meet
// another plugin's at link time. NF_Noise.cpp and NF_Spat_Data.cpp both
// define an inline hash_01 with different bodies, so NF_Spat_Data.cpp has
// its own unit (see grain_unit_2.cpp). The entry points have C linkage,
// which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_grain_EffectMain
#define PluginDataEntryFunction2 fsp_grain_PluginDataEntryFunction2

namespace fsp_grain {
#include "_NFLib/fx/NF_Noise.cpp"
#include "Grain/Grain.cpp"
}  // namespace fsp_grain
