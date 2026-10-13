// SelectiveColorBlur, first of four units in one namespace: its sources
// define the same file-local helpers, so each source gets a unit of its
// own. The entry points have C linkage, which ignores namespaces: they
// are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_selectivecolorblur_EffectMain
#define PluginDataEntryFunction2 \
  fsp_selectivecolorblur_PluginDataEntryFunction2

namespace fsp_selectivecolorblur {
#include "SelectiveColorBlur/SelectiveColorBlur.cpp"
}  // namespace fsp_selectivecolorblur
