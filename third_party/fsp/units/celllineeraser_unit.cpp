// CellLineEraser as one translation unit in a namespace of its own, so
// its helpers never meet another plugin's at link time. The entry
// points have C linkage, which ignores namespaces: they are renamed
// instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_celllineeraser_EffectMain
#define PluginDataEntryFunction2 fsp_celllineeraser_PluginDataEntryFunction2

namespace fsp_celllineeraser {
#include "CellLineEraser/CellLineEraser.cpp"
#include "CellLineEraser/CellLineEraserSub.cpp"
}  // namespace fsp_celllineeraser
