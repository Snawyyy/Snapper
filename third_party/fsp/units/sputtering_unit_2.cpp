// Sputtering, part 2 of 2: NF_SpatUtils.cpp (see sputtering_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_sputtering_EffectMain
#define PluginDataEntryFunction2 fsp_sputtering_PluginDataEntryFunction2

namespace fsp_sputtering {
#include "_NFLib/fx/NF_SpatUtils.cpp"
}  // namespace fsp_sputtering
