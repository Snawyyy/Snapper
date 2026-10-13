// Outline in a namespace of its own, so its helpers never meet another
// plugin's at link time. The entry points have C linkage, which ignores
// namespaces: they are renamed instead. NF_Blur and NF_ChannelBlur (and
// NF_MinMax and NF_ChannelMinMax) share file-local names, so the
// sources are split over outline_unit_1.cpp and outline_unit_2.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_outline_EffectMain
#define PluginDataEntryFunction2 fsp_outline_PluginDataEntryFunction2

namespace fsp_outline {
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "Outline/Outline.cpp"
#include "Outline/OutlineFilter.cpp"
}  // namespace fsp_outline
