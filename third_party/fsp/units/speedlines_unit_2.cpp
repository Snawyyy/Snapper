// SpeedLines' A_Noise.cpp, apart from speedlines_unit_1.cpp because it
// defines the same file-local helper as NF_Blend.cpp. Same prelude,
// renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_speedlines_EffectMain
#define PluginDataEntryFunction2 fsp_speedlines_PluginDataEntryFunction2

namespace fsp_speedlines {
#include "SpeedLines/A_Noise.cpp"
}  // namespace fsp_speedlines
