// InkingLineBlur in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. NF_SelectPixels.cpp and
// NF_TargetPixels.cpp share file-local names (ECTraits, SPInfo,
// SelectPixels_Template), so each has a unit of its own
// (inkinglineblur_unit_2.cpp and _3.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_inkinglineblur_EffectMain
#define PluginDataEntryFunction2 fsp_inkinglineblur_PluginDataEntryFunction2

namespace fsp_inkinglineblur {
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "InkingLineBlur/InkingLineBlur.cpp"
}  // namespace fsp_inkinglineblur
