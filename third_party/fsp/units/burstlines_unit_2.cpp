// BurstLines' NF_UnMult.cpp, apart from burstlines_unit_1.cpp because
// it defines the same file-local helpers as NF_Mult.cpp. Same prelude,
// renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_burstlines_EffectMain
#define PluginDataEntryFunction2 fsp_burstlines_PluginDataEntryFunction2

namespace fsp_burstlines {
#include "_NFLib/fx/NF_UnMult.cpp"
}  // namespace fsp_burstlines
