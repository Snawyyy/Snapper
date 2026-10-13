// Sputtering, part 1 of 2, in a namespace of its own so its helpers never
// meet another plugin's at link time. NF_Blend.cpp and NF_SpatUtils.cpp
// both specialise PixelTraits (with different members), so
// NF_SpatUtils.cpp has its own unit. The entry points have C linkage,
// which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_sputtering_EffectMain
#define PluginDataEntryFunction2 fsp_sputtering_PluginDataEntryFunction2

namespace fsp_sputtering {
#include "_NFLib/fx/NF_Blend.cpp"
#include "Sputtering/SputMain.cpp"
#include "Sputtering/Sputtering.cpp"
#include "Sputtering/TargetWorld.cpp"
}  // namespace fsp_sputtering
