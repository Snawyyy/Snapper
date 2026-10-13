// ColorSwitch as one translation unit in a namespace of its own, so its
// helpers never meet another plugin's at link time. The entry points
// have C linkage, which ignores namespaces: they are renamed instead.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_colorswitch_EffectMain
#define PluginDataEntryFunction2 fsp_colorswitch_PluginDataEntryFunction2

namespace fsp_colorswitch {
#include "ColorSwitch/ColorSwitch.cpp"
#include "ColorSwitch/ColoeSwitchSub.cpp"
#include "ColorSwitch/ColorSwitchParams.cpp"
}  // namespace fsp_colorswitch
