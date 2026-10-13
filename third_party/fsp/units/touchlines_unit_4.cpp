// TouchLines' TouchLineFilter.cpp, apart from touchlines_unit_1.cpp
// because it defines the same file-local helper as ThresholdLine.cpp.
// Same prelude, renames and namespace.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_touchlines_EffectMain
#define PluginDataEntryFunction2 fsp_touchlines_PluginDataEntryFunction2

namespace fsp_touchlines {
#include "TouchLines/TouchLineFilter.cpp"
}  // namespace fsp_touchlines
