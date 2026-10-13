// Thin as one translation unit in a namespace of its own, so its
// helpers never meet another plugin's at link time. The entry points
// have C linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_thin_EffectMain
#define PluginDataEntryFunction2 fsp_thin_PluginDataEntryFunction2

namespace fsp_thin {
#include "Thin/Thin.cpp"
#include "Thin/Thinsub.cpp"
}  // namespace fsp_thin
