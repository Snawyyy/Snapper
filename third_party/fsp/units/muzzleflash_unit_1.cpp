// MuzzleFlash in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. Sources that define the
// same file-local helpers as one here sit in muzzleflash_unit_<n>.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_muzzleflash_EffectMain
#define PluginDataEntryFunction2 fsp_muzzleflash_PluginDataEntryFunction2

namespace fsp_muzzleflash {
#include "_NFLib/fx/debug_font.cpp"
#include "_NFLib/fx/NF_AlphaHyperbolic.cpp"
#include "_NFLib/fx/NF_Bilateral.cpp"
#include "_NFLib/fx/NF_Blur.cpp"
#include "_NFLib/fx/NF_ChannelMinMax.cpp"
#include "_NFLib/fx/NF_Draw.cpp"
#include "_NFLib/fx/NF_DrawAALine.cpp"
#include "_NFLib/fx/NF_DrawWorld.cpp"
#include "_NFLib/fx/NF_Gradient.cpp"
#include "_NFLib/fx/NF_LogicGray.cpp"
#include "_NFLib/fx/NF_Mosaic.cpp"
#include "_NFLib/fx/NF_Mult.cpp"
#include "_NFLib/fx/NF_Noise.cpp"
#include "_NFLib/fx/NF_Paint.cpp"
#include "_NFLib/fx/NF_SpatUtils.cpp"
#include "_NFLib/vector/NF_VectorLine.cpp"
#include "MuzzleFlash/MuzzleFlash.cpp"
#include "MuzzleFlash/MuzzleFlashMain.cpp"
}  // namespace fsp_muzzleflash
