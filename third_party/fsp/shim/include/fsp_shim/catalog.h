// The plugins as the C interface lists them: each one's names and
// parameters, read once by running its GLOBAL_SETUP and PARAMS_SETUP.

#ifndef FSP_SHIM_CATALOG_H_
#define FSP_SHIM_CATALOG_H_

#include <memory>
#include <string>
#include <vector>

#include "fsp_host.h"
#include "fsp_shim/registry.h"
#include "fsp_shim/session.h"

namespace fsp_shim {

// Held by unique_ptr only: it owns global_data, freed with it.
struct CatalogPlugin {
  CatalogPlugin() = default;
  ~CatalogPlugin();

  const FspPluginEntry* entry = nullptr;
  std::string name;
  std::string match_name;
  std::string category;
  // The plugin's own definitions, parameter 1 on (0 is the input).
  std::vector<FspShimRecordedParam> defs;
  std::vector<std::vector<std::string>> choice_texts;
  std::vector<std::vector<const char*>> choice_ptrs;
  std::vector<FspHostParam> params;
  int slot_count = 0;
  // What GLOBAL_SETUP left in out_data->global_data.
  PF_Handle global_data = nullptr;
};

// The plugins whose setup ran cleanly, built on first use.
const std::vector<std::unique_ptr<CatalogPlugin>>& Catalog();

// in_data for one call into a plugin, as After Effects fills it for an
// 8 bit layer of width x height at the given downsampling scale.
PF_InData MakeInData(FspShimEffectRef* ref, PF_Handle global_data,
                     A_long width, A_long height, double scale);

// Calls the plugin's EffectMain, turning any escaping exception into an
// error (the plugins catch only PF_Err themselves).
PF_Err CallEffect(const FspPluginEntry& entry, PF_Cmd cmd,
                  PF_InData* in_data, PF_OutData* out_data,
                  PF_ParamDef* params[], PF_LayerDef* output);

}  // namespace fsp_shim

#endif  // FSP_SHIM_CATALOG_H_
