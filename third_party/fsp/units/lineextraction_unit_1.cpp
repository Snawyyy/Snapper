// LineExtraction in a namespace of its own, so its helpers never meet
// another plugin's at link time. The entry points have C linkage, which
// ignores namespaces: they are renamed instead. Its sources define the
// same file-local helpers (PixelTraits, GetMaxChannel) more than once,
// so they are split over lineextraction_unit_1.cpp to _3.cpp.

#include "fsp_shim/unit_prelude.h"

#define EffectMain fsp_lineextraction_EffectMain
#define PluginDataEntryFunction2 fsp_lineextraction_PluginDataEntryFunction2

namespace fsp_lineextraction {
#include "_NFLib/fx/NF_ChannelMinMax.cpp"
#include "LineExtraction/LineExtraction.cpp"
#include "LineExtraction/LineExtractionFilter.cpp"
}  // namespace fsp_lineextraction
