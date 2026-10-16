// MuzzleFlash's NF_Blend.cpp, apart from muzzleflash_unit_1.cpp because
// it defines the same file-local helpers as NF_SpatUtils.cpp. Same
// prelude, renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_muzzleflash_EffectMain
#define PluginDataEntryFunction2 fsp_muzzleflash_PluginDataEntryFunction2

namespace fsp_muzzleflash {
#include "_NFLib/fx/NF_Blend.cpp"
}  // namespace fsp_muzzleflash
