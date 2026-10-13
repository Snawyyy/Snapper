// AlphaFill, first of three units in one namespace: its sources each
// define the same file-local helpers (ChannelTypeHelper, GetMaxChannel),
// so each source gets a unit of its own. The entry points have C
// linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_alphafill_EffectMain
#define PluginDataEntryFunction2 fsp_alphafill_PluginDataEntryFunction2

namespace fsp_alphafill {
#include "AlphaFill/AlphaFill.cpp"
}  // namespace fsp_alphafill
