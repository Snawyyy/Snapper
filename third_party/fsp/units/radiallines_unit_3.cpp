// RadialLines' NF_ChannelBlur.cpp, apart from radiallines_unit_1.cpp
// because it defines the same file-local helpers as NF_Blur.cpp. Same
// prelude, renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_radiallines_EffectMain
#define PluginDataEntryFunction2 fsp_radiallines_PluginDataEntryFunction2

namespace fsp_radiallines {
#include "_NFLib/fx/NF_ChannelBlur.cpp"
}  // namespace fsp_radiallines
