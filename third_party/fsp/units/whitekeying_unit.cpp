// WhiteKeying as one translation unit in a namespace of its own, so its
// helpers never meet another plugin's at link time. The entry points
// have C linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_whitekeying_EffectMain
#define PluginDataEntryFunction2 fsp_whitekeying_PluginDataEntryFunction2

namespace fsp_whitekeying {
#include "WhiteKeying/WhiteKeying.cpp"
#include "WhiteKeying/WhiteKeyingFilter.cpp"
}  // namespace fsp_whitekeying
