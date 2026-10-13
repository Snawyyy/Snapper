// RimFill as one translation unit in a namespace of its own, so its
// helpers never meet another plugin's at link time. The entry points
// have C linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_rimfill_EffectMain
#define PluginDataEntryFunction2 fsp_rimfill_PluginDataEntryFunction2

namespace fsp_rimfill {
#include "RimFill/RimFill.cpp"
#include "RimFill/RimFillSub.cpp"
#include "RimFill/RimFillFilter.cpp"
}  // namespace fsp_rimfill
