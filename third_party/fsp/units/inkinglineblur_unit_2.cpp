// InkingLineBlur's NF_SelectPixels.cpp (see inkinglineblur_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_inkinglineblur_EffectMain
#define PluginDataEntryFunction2 fsp_inkinglineblur_PluginDataEntryFunction2

namespace fsp_inkinglineblur {
#include "InkingLineBlur/NF_SelectPixels.cpp"
}  // namespace fsp_inkinglineblur
