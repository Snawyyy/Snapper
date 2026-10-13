// InnerFlare, part 2 of 3: AlphaCopy.cpp (see innerflare_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_innerflare_EffectMain
#define PluginDataEntryFunction2 fsp_innerflare_PluginDataEntryFunction2

namespace fsp_innerflare {
#include "InnerFlare/AlphaCopy.cpp"
}  // namespace fsp_innerflare
