// One render call into a plugin, and the C interface over the catalog.

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "fsp_host.h"
#include "fsp_shim/ae_all.h"
#include "fsp_shim/catalog.h"

namespace fsp_shim {
namespace {

constexpr long long kMaxPixels = 1LL << 28;

// The closest fraction with a small denominator, so plugin code that
// scales integers by num / den neither overflows nor drifts.
PF_RationalScale ToRational(double scale) {
  PF_RationalScale best{1, 1};
  double best_miss = std::numeric_limits<double>::max();
  for (A_long den = 1; den <= 64; ++den) {
    const A_long num = std::lround(scale * static_cast<double>(den));
    const double miss = std::fabs(static_cast<double>(num) / den - scale);
    const bool is_closer = num > 0 && miss < best_miss - 1e-12;
    if (is_closer) {
      best = PF_RationalScale{num, static_cast<A_u_long>(den)};
      best_miss = miss;
    }
  }
  return best;
}

double Clamp(double value, const FspHostParam& param) {
  const bool is_number = std::isfinite(value);
  const double v = is_number ? value : param.neutral;
  return v < param.low ? param.low : (v > param.high ? param.high : v);
}

// Puts the value(s) at param's slot into def, in the plugin's units.
void ApplyValue(const FspHostParam& param, const double* values,
                A_long width, A_long height, PF_ParamDef* def) {
  const double v = Clamp(values[param.slot], param);
  switch (def->param_type) {
    case PF_Param_SLIDER:
      def->u.sd.value = std::lround(v);
      break;
    case PF_Param_FIX_SLIDER:
      def->u.fd.value = FLOAT2FIX(v);
      break;
    case PF_Param_FLOAT_SLIDER:
      def->u.fs_d.value = v;
      break;
    case PF_Param_ANGLE:
      def->u.ad.value = FLOAT2FIX(v);
      break;
    case PF_Param_CHECKBOX:
      def->u.bd.value = v != 0.0 ? 1 : 0;
      break;
    case PF_Param_POPUP:
      def->u.pd.value = std::lround(v);
      break;
    case PF_Param_COLOR: {
      const long rgb = std::lround(v);
      def->u.cd.value = PF_Pixel{255, static_cast<A_u_char>(rgb >> 16),
                                 static_cast<A_u_char>(rgb >> 8),
                                 static_cast<A_u_char>(rgb)};
      break;
    }
    case PF_Param_POINT: {
      // Percent of the picture, to pixels of the picture as it is now.
      const double y = Clamp(values[param.slot + 1], param);
      def->u.td.x_value = FLOAT2FIX(v / 100.0 * width);
      def->u.td.y_value = FLOAT2FIX(y / 100.0 * height);
      break;
    }
    case PF_Param_LAYER:
      // No layer picked, as After Effects shows "None".
      std::memset(&def->u.ld, 0, sizeof(def->u.ld));
      break;
    default:
      break;
  }
}

std::string Describe(const char* id, PF_Err err, const PF_OutData& out) {
  std::string text = std::string(id) + " failed with error " +
                     std::to_string(err);
  const bool has_message = out.return_msg[0] != '\0';
  if (has_message) {
    text += std::string(" (") + out.return_msg + ")";
  }
  const std::string shim = LastErrorText();
  const bool has_shim = !shim.empty();
  if (has_shim) {
    text += ": " + shim;
  }
  return text;
}

PF_Err Render(const CatalogPlugin& plugin, A_long width, A_long height,
              const unsigned char* argb_in, unsigned char* argb_out,
              const double* values, double scale) {
  FspShimEffectRef ref;
  PF_EffectWorld input{};
  PF_EffectWorld output{};
  PF_Err err = NewWorld(&ref, width, height, PF_PixelFormat_ARGB32, &input);
  ERR(NewWorld(&ref, width, height, PF_PixelFormat_ARGB32, &output));
  const bool has_no_worlds = err != PF_Err_NONE;
  if (has_no_worlds) {
    return err;
  }
  const std::size_t bytes = static_cast<std::size_t>(width) *
                            static_cast<std::size_t>(height) * 4;
  std::memcpy(input.data, argb_in, bytes);
  ref.params.resize(plugin.defs.size() + 1);
  ref.params[0].param_type = PF_Param_LAYER;
  ref.params[0].u.ld = input;
  std::vector<PF_ParamDef*> params{&ref.params[0]};
  for (std::size_t i = 0; i < plugin.defs.size(); ++i) {
    PF_ParamDef& def = ref.params[i + 1];
    def = plugin.defs[i].def;
    ApplyValue(plugin.params[i], values, width, height, &def);
    params.push_back(&def);
  }
  PF_InData in_data =
      MakeInData(&ref, plugin.global_data, width, height, scale);
  PF_OutData out_data{};
  err = CallEffect(*plugin.entry, PF_Cmd_RENDER, &in_data, &out_data,
                   params.data(), &output);
  const bool has_failed = err != PF_Err_NONE;
  if (has_failed) {
    SetLastError(Describe(plugin.entry->id, err, out_data));
    return err;
  }
  std::memcpy(argb_out, output.data, bytes);
  return PF_Err_NONE;
}

const CatalogPlugin* PluginAt(int plugin) {
  const auto& catalog = Catalog();
  const bool is_in_range =
      plugin >= 0 && plugin < static_cast<int>(catalog.size());
  return is_in_range ? catalog[static_cast<std::size_t>(plugin)].get()
                     : nullptr;
}

}  // namespace

PF_InData MakeInData(FspShimEffectRef* ref, PF_Handle global_data,
                     A_long width, A_long height, double scale) {
  PF_InData in{};
  in.inter = InteractCallbacks();
  in.utils = const_cast<PF_UtilCallbacks*>(UtilCallbacks());
  in.effect_ref = ref;
  in.quality = PF_Quality_HI;
  in.version = PF_SpecVersion{13, 28};
  in.appl_id = 'FXTC';
  in.num_params = static_cast<A_long>(ref->params.size());
  // One frame at 24 frames a second, at time 0.
  in.current_time = 0;
  in.time_step = 1;
  in.local_time_step = 1;
  in.total_time = 1;
  in.time_scale = 24;
  in.field = PF_Field_FRAME;
  const PF_RationalScale down = ToRational(scale);
  in.width = std::lround(width / scale);
  in.height = std::lround(height / scale);
  in.extent_hint = PF_Rect{0, 0, width, height};
  in.downsample_x = down;
  in.downsample_y = down;
  in.pixel_aspect_ratio = PF_RationalScale{1, 1};
  in.global_data = global_data;
  in.pica_basicP = const_cast<SPBasicSuite*>(&fsp_shim_basic_suite);
  return in;
}

PF_Err CallEffect(const FspPluginEntry& entry, PF_Cmd cmd,
                  PF_InData* in_data, PF_OutData* out_data,
                  PF_ParamDef* params[], PF_LayerDef* output) {
  try {
    return entry.effect_main(cmd, in_data, out_data, params, output,
                             nullptr);
  } catch (PF_Err thrown) {
    return thrown;
  } catch (...) {
    return fsp_shim_fail("the plugin threw an unknown exception");
  }
}

}  // namespace fsp_shim

using fsp_shim::PluginAt;

int fsp_host_plugin_count(void) {
  return static_cast<int>(fsp_shim::Catalog().size());
}

const char* fsp_host_plugin_id(int plugin) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  return found != nullptr ? found->entry->id : nullptr;
}

const char* fsp_host_plugin_name(int plugin) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  return found != nullptr ? found->name.c_str() : nullptr;
}

const char* fsp_host_plugin_category(int plugin) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  return found != nullptr ? found->category.c_str() : nullptr;
}

int fsp_host_param_count(int plugin) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  return found != nullptr ? static_cast<int>(found->params.size()) : 0;
}

const FspHostParam* fsp_host_param(int plugin, int param) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  const bool is_in_range =
      found != nullptr && param >= 0 &&
      param < static_cast<int>(found->params.size());
  return is_in_range ? &found->params[static_cast<std::size_t>(param)]
                     : nullptr;
}

int fsp_host_slot_count(int plugin) {
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  return found != nullptr ? found->slot_count : 0;
}

int fsp_host_render(int plugin, int width, int height,
                    const unsigned char* argb_in, unsigned char* argb_out,
                    const double* values, int value_count, double scale) {
  fsp_shim::ClearLastError();
  const fsp_shim::CatalogPlugin* found = PluginAt(plugin);
  const long long pixels = static_cast<long long>(width) * height;
  const bool is_bad = found == nullptr || width <= 0 || height <= 0 ||
                      pixels > fsp_shim::kMaxPixels || argb_in == nullptr ||
                      argb_out == nullptr || argb_in == argb_out ||
                      !(scale > 0.0 && scale <= 16.0) ||
                      (values == nullptr && value_count > 0);
  if (is_bad) {
    fsp_shim::SetLastError("fsp_host_render: bad plugin, size or buffers");
    return PF_Err_BAD_CALLBACK_PARAM;
  }
  const bool is_miscounted = value_count != found->slot_count;
  if (is_miscounted) {
    fsp_shim::SetLastError(std::string(found->entry->id) + " takes " +
                           std::to_string(found->slot_count) +
                           " values, got " + std::to_string(value_count));
    return PF_Err_BAD_CALLBACK_PARAM;
  }
  const PF_Err err = fsp_shim::Render(*found, width, height, argb_in,
                                      argb_out, values, scale);
  return static_cast<int>(err);
}

const char* fsp_host_last_error(void) { return fsp_shim::LastErrorText(); }
