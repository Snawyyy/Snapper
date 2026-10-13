// The plugins Snapper carries. Adding one is one line in FSP_PLUGINS
// (its folder name, then the prefix its unit gives EffectMain and
// PluginDataEntryFunction2 in units/<prefix>_unit.cpp).

#include "fsp_shim/registry.h"

#include <cassert>

#define FSP_PLUGINS(PLUGIN) \
  PLUGIN(RimFill, rimfill) \
  PLUGIN(Outline, outline) \
  PLUGIN(Thin, thin)

#define FSP_DECLARE_PLUGIN(ID, PREFIX) \
  extern "C" PF_Err fsp_##PREFIX##_EffectMain( \
      PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, \
      PF_ParamDef* params[], PF_LayerDef* output, void* extra); \
  extern "C" PF_Err fsp_##PREFIX##_PluginDataEntryFunction2( \
      PF_PluginDataPtr data, PF_PluginDataCB2 callback, \
      SPBasicSuite* basic_suite, const char* host_name, \
      const char* host_version);
FSP_PLUGINS(FSP_DECLARE_PLUGIN)

#define FSP_LIST_PLUGIN(ID, PREFIX) \
  {#ID, fsp_##PREFIX##_EffectMain, fsp_##PREFIX##_PluginDataEntryFunction2},

namespace {

constexpr FspPluginEntry kPlugins[] = {FSP_PLUGINS(FSP_LIST_PLUGIN)};

}  // namespace

const FspPluginEntry* FspPluginEntries(int* count) {
  assert(count != nullptr);
  *count = static_cast<int>(sizeof(kPlugins) / sizeof(kPlugins[0]));
  assert(*count > 0);
  return kPlugins;
}
