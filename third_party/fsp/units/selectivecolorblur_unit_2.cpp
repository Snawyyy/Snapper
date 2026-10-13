// SelectiveColorBlur, second unit: the blur (see
// selectivecolorblur_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_selectivecolorblur_EffectMain
#define PluginDataEntryFunction2 \
  fsp_selectivecolorblur_PluginDataEntryFunction2

namespace fsp_selectivecolorblur {
#include "SelectiveColorBlur/SelectiveColorBlurSub.cpp"
}  // namespace fsp_selectivecolorblur
