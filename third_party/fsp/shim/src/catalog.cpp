// Builds the plugin list: runs each plugin's registration, GLOBAL_SETUP
// and PARAMS_SETUP once and turns its parameter definitions into the C
// interface's FspHostParam records.

#include "fsp_shim/catalog.h"

#include <cstring>
#include <string>

#include "fsp_shim/ae_all.h"

namespace fsp_shim {
namespace {

// How far an angle or a point may be typed (After Effects sets none).
constexpr double kAngleTurns = 3600.0;
constexpr double kPointReach = 1000.0;

PF_Err RecordNames(PF_PluginDataPtr data, const A_u_char* name,
                   const A_u_char* match_name, const A_u_char* category,
                   const A_u_char*, A_long, A_long, A_long, A_long,
                   const A_u_char*) {
  CatalogPlugin* plugin = reinterpret_cast<CatalogPlugin*>(data);
  auto text = [](const A_u_char* s) {
    return s != nullptr ? std::string(reinterpret_cast<const char*>(s))
                        : std::string();
  };
  plugin->name = text(name);
  plugin->match_name = text(match_name);
  plugin->category = text(category);
  return PF_Err_NONE;
}

std::vector<std::string> SplitChoices(const std::string& items, int count) {
  std::vector<std::string> choices;
  std::size_t start = 0;
  for (int i = 0; i < count; ++i) {
    const std::size_t bar = items.find('|', start);
    const bool is_last = bar == std::string::npos;
    choices.push_back(items.substr(start, is_last ? std::string::npos
                                                  : bar - start));
    start = is_last ? items.size() : bar + 1;
  }
  return choices;
}

void SetRange(FspHostParam* param, double low, double high,
              double slider_low, double slider_high, double neutral) {
  param->low = low;
  param->high = high;
  param->slider_low = slider_low;
  param->slider_high = slider_high;
  param->neutral = neutral;
}

// Fills type, range and default of param from the plugin's definition.
void Describe(const PF_ParamDef& def, FspHostParam* param) {
  const double fix = 65536.0;
  switch (def.param_type) {
    case PF_Param_SLIDER: {
      const PF_SliderDef& d = def.u.sd;
      param->type = FSP_HOST_PARAM_SLIDER;
      SetRange(param, d.valid_min, d.valid_max, d.slider_min, d.slider_max,
               d.dephault);
      break;
    }
    case PF_Param_FIX_SLIDER: {
      const PF_FixedSliderDef& d = def.u.fd;
      param->type = FSP_HOST_PARAM_FLOAT;
      SetRange(param, d.valid_min / fix, d.valid_max / fix,
               d.slider_min / fix, d.slider_max / fix, d.dephault / fix);
      break;
    }
    case PF_Param_FLOAT_SLIDER: {
      const PF_FloatSliderDef& d = def.u.fs_d;
      param->type = FSP_HOST_PARAM_FLOAT;
      SetRange(param, d.valid_min, d.valid_max, d.slider_min, d.slider_max,
               d.dephault);
      break;
    }
    case PF_Param_ANGLE:
      param->type = FSP_HOST_PARAM_ANGLE;
      SetRange(param, -kAngleTurns, kAngleTurns, -180.0, 180.0,
               def.u.ad.dephault / fix);
      break;
    case PF_Param_CHECKBOX:
      param->type = FSP_HOST_PARAM_CHECKBOX;
      SetRange(param, 0, 1, 0, 1, def.u.bd.dephault != 0 ? 1 : 0);
      break;
    case PF_Param_POPUP:
      param->type = FSP_HOST_PARAM_POPUP;
      SetRange(param, 1, def.u.pd.num_choices, 1, def.u.pd.num_choices,
               def.u.pd.dephault);
      break;
    case PF_Param_COLOR: {
      const PF_Pixel& c = def.u.cd.dephault;
      param->type = FSP_HOST_PARAM_COLOR;
      SetRange(param, 0, 0xFFFFFF, 0, 0xFFFFFF,
               (c.red << 16) | (c.green << 8) | c.blue);
      break;
    }
    case PF_Param_POINT: {
      const bool is_bounded = def.u.td.restrict_bounds != 0;
      const double low = is_bounded ? 0.0 : -kPointReach;
      const double high = is_bounded ? 100.0 : 100.0 + kPointReach;
      param->type = FSP_HOST_PARAM_POINT;
      param->slot_count = 2;
      SetRange(param, low, high, 0.0, 100.0, def.u.td.x_dephault / fix);
      param->neutral_y = def.u.td.y_dephault / fix;
      break;
    }
    default:
      param->type = FSP_HOST_PARAM_OTHER;
      param->is_hidden = 1;
      break;
  }
  const bool is_invisible = (def.ui_flags & PF_PUI_INVISIBLE) != 0;
  if (is_invisible) {
    param->is_hidden = 1;
  }
}

void BuildParams(CatalogPlugin* plugin) {
  const std::size_t count = plugin->defs.size();
  plugin->choice_texts.resize(count);
  plugin->choice_ptrs.resize(count);
  plugin->params.resize(count);
  int slot = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const FspShimRecordedParam& recorded = plugin->defs[i];
    FspHostParam& param = plugin->params[i];
    param = FspHostParam{};
    param.name = recorded.def.name;
    param.slot = slot;
    param.slot_count = 1;
    Describe(recorded.def, &param);
    const bool is_popup = recorded.def.param_type == PF_Param_POPUP;
    if (is_popup) {
      plugin->choice_texts[i] =
          SplitChoices(recorded.items, recorded.def.u.pd.num_choices);
      for (const std::string& text : plugin->choice_texts[i]) {
        plugin->choice_ptrs[i].push_back(text.c_str());
      }
      param.choice_count = static_cast<int>(plugin->choice_ptrs[i].size());
      param.choices = plugin->choice_ptrs[i].data();
    }
    slot += param.slot_count;
  }
  plugin->slot_count = slot;
}

// Runs registration and both setups; false (with the reason printed)
// when the plugin cannot be offered.
bool SetUp(CatalogPlugin* plugin) {
  const FspPluginEntry& entry = *plugin->entry;
  const PF_Err named = entry.data_entry(
      reinterpret_cast<PF_PluginDataPtr>(plugin), RecordNames,
      const_cast<SPBasicSuite*>(&fsp_shim_basic_suite), "Snapper", "1");
  FspShimEffectRef ref;
  ref.params.resize(1);
  ref.params[0].param_type = PF_Param_LAYER;
  PF_ParamDef* params[] = {&ref.params[0]};
  PF_InData in_data = MakeInData(&ref, nullptr, 1, 1, 1.0);
  PF_OutData out_data{};
  const PF_Err global =
      CallEffect(entry, PF_Cmd_GLOBAL_SETUP, &in_data, &out_data, params,
                 nullptr);
  plugin->global_data = out_data.global_data;
  in_data.global_data = plugin->global_data;
  ref.is_recording = true;
  const PF_Err setup = CallEffect(entry, PF_Cmd_PARAMS_SETUP, &in_data,
                                  &out_data, params, nullptr);
  ref.is_recording = false;
  const bool has_failed = named != PF_Err_NONE ||
                          global != PF_Err_NONE || setup != PF_Err_NONE;
  const A_long declared = static_cast<A_long>(ref.recorded.size()) + 1;
  const bool is_miscounted = out_data.num_params != declared;
  const bool is_unusable = has_failed || is_miscounted;
  if (is_unusable) {
    SetLastError(std::string(entry.id) + ": setup failed (errors " +
                 std::to_string(named) + ", " + std::to_string(global) +
                 ", " + std::to_string(setup) + "; " +
                 std::to_string(out_data.num_params) + " parameters said, " +
                 std::to_string(declared) + " made); left out");
    return false;
  }
  plugin->defs = std::move(ref.recorded);
  BuildParams(plugin);
  return true;
}

std::vector<std::unique_ptr<CatalogPlugin>> BuildCatalog() {
  int count = 0;
  const FspPluginEntry* entries = FspPluginEntries(&count);
  std::vector<std::unique_ptr<CatalogPlugin>> plugins;
  for (int i = 0; i < count; ++i) {
    auto plugin = std::make_unique<CatalogPlugin>();
    plugin->entry = &entries[i];
    const bool is_ready = SetUp(plugin.get());
    if (is_ready) {
      plugins.push_back(std::move(plugin));
    }
  }
  ClearLastError();
  return plugins;
}

}  // namespace

CatalogPlugin::~CatalogPlugin() { fsp_shim_dispose_handle(global_data); }

const std::vector<std::unique_ptr<CatalogPlugin>>& Catalog() {
  static const std::vector<std::unique_ptr<CatalogPlugin>> catalog =
      BuildCatalog();
  return catalog;
}

}  // namespace fsp_shim
