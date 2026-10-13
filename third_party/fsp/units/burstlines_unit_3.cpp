// BurstLines' NF_VectorMask.cpp, apart from burstlines_unit_1.cpp
// because it defines the same file-local helpers as NF_Blend.cpp. Same
// prelude, renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_burstlines_EffectMain
#define PluginDataEntryFunction2 fsp_burstlines_PluginDataEntryFunction2

namespace fsp_burstlines {
#include "_NFLib/vector/NF_VectorMask.cpp"
}  // namespace fsp_burstlines
