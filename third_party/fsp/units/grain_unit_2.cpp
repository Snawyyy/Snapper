// Grain, part 2 of 2: NF_Spat_Data.cpp (see grain_unit_1.cpp). Its inline
// hash_01 differs from NF_Noise.cpp's; two inline functions of one name
// in one namespace would be merged by the linker and one file would hash
// with the other's formula. Renaming it here keeps each file on its own.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_grain_EffectMain
#define PluginDataEntryFunction2 fsp_grain_PluginDataEntryFunction2
#define hash_01 fsp_grain_spat_hash_01

namespace fsp_grain {
#include "_NFLib/fx/NF_Spat_Data.cpp"
}  // namespace fsp_grain
