// AlphaFill, third unit: the color key mode (see alphafill_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_alphafill_EffectMain
#define PluginDataEntryFunction2 fsp_alphafill_PluginDataEntryFunction2

namespace fsp_alphafill {
#include "AlphaFill/ChromaKey.cpp"
}  // namespace fsp_alphafill
