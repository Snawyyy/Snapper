// InnerFlareRot, part 1 of 4, in a namespace of its own so its helpers
// never meet another plugin's at link time. Its sources share file-local
// names (AlpheCopyInfo in the AlphaCopy sources, BlurInfo and LineBuf in
// the plain and Channel blur and minmax), so those sit in units of their
// own. The entry points have C linkage, which ignores namespaces: they
// are renamed.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_innerflarerot_EffectMain
#define PluginDataEntryFunction2 fsp_innerflarerot_PluginDataEntryFunction2

namespace fsp_innerflarerot {
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Blend.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_MinMax.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "InnerFlareRot/InnerFlareRot.cpp"
}  // namespace fsp_innerflarerot
