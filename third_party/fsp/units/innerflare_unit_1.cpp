// InnerFlare, part 1 of 3, in a namespace of its own so its helpers never
// meet another plugin's at link time. Its sources share file-local names
// (AlpheCopyInfo), so each AlphaCopy source has its own unit. The entry
// points have C linkage, which ignores namespaces: they are renamed.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_innerflare_EffectMain
#define PluginDataEntryFunction2 fsp_innerflare_PluginDataEntryFunction2

namespace fsp_innerflare {
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "InnerFlare/InnerFlare.cpp"
}  // namespace fsp_innerflare
