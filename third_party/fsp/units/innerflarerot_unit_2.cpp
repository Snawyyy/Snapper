// InnerFlareRot, part 2 of 4: the Channel blur and minmax (see
// innerflarerot_unit_1.cpp).

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_innerflarerot_EffectMain
#define PluginDataEntryFunction2 fsp_innerflarerot_PluginDataEntryFunction2

namespace fsp_innerflarerot {
#include "_NFLib/fx/NF_ChannelBlur.cpp"
#include "_NFLib/fx/NF_ChannelMinMax.cpp"
}  // namespace fsp_innerflarerot
