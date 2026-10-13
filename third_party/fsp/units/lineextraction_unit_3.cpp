// More of LineExtraction's sources (see lineextraction_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_lineextraction_EffectMain
#define PluginDataEntryFunction2 fsp_lineextraction_PluginDataEntryFunction2

namespace fsp_lineextraction {
#include "_NFLib/fx/NF_Blend.cpp"
}  // namespace fsp_lineextraction
