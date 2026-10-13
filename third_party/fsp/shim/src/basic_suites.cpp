// The basic suite lookup (pica_basicP), the C library suite, the AEGP
// suites and the interact callbacks of the stand-in host.

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "fsp_shim/ae_all.h"
#include "fsp_shim/session.h"

namespace {

// ---------------------------------------------------------------------
// The C library, wrapped: the standard does not allow taking the
// address of its functions.
A_FpLong Atan(A_FpLong x) { return std::atan(x); }
A_FpLong Atan2(A_FpLong y, A_FpLong x) { return std::atan2(y, x); }
A_FpLong Ceil(A_FpLong x) { return std::ceil(x); }
A_FpLong Cos(A_FpLong x) { return std::cos(x); }
A_FpLong Exp(A_FpLong x) { return std::exp(x); }
A_FpLong Fabs(A_FpLong x) { return std::fabs(x); }
A_FpLong Floor(A_FpLong x) { return std::floor(x); }
A_FpLong Fmod(A_FpLong x, A_FpLong y) { return std::fmod(x, y); }
A_FpLong Hypot(A_FpLong x, A_FpLong y) { return std::hypot(x, y); }
A_FpLong Log(A_FpLong x) { return std::log(x); }
A_FpLong Log10(A_FpLong x) { return std::log10(x); }
A_FpLong Pow(A_FpLong x, A_FpLong y) { return std::pow(x, y); }
A_FpLong Sin(A_FpLong x) { return std::sin(x); }
A_FpLong Sqrt(A_FpLong x) { return std::sqrt(x); }
A_FpLong Tan(A_FpLong x) { return std::tan(x); }
A_FpLong Asin(A_FpLong x) { return std::asin(x); }
A_FpLong Acos(A_FpLong x) { return std::acos(x); }
A_char* Strcpy(A_char* dst, const A_char* src) {
  return std::strcpy(dst, src);
}
int Sprintf(A_char* dst, const A_char* format, ...) {
  va_list args;
  va_start(args, format);
  const int written = std::vsprintf(dst, format, args);
  va_end(args);
  return written;
}

// ---------------------------------------------------------------------
// The basic suite: suites by name and version.
struct NamedSuite {
  const char* name;
  A_long version;
  const void* suite;
};

const NamedSuite kSuites[] = {
    {kPFIterate8Suite, 1, &fsp_shim_iterate8_suite},
    {kPFIterate8Suite, 2, &fsp_shim_iterate8_suite},
    {kPFIterate16Suite, 1, &fsp_shim_iterate16_suite},
    {kPFIterate16Suite, 2, &fsp_shim_iterate16_suite},
    {kPFIterateFloatSuite, 1, &fsp_shim_iterate_float_suite},
    {kPFIterateFloatSuite, 2, &fsp_shim_iterate_float_suite},
    {kPFWorldSuite, 1, &fsp_shim_world_suite1},
    {kPFWorldSuite, 2, &fsp_shim_world_suite2},
    {kPFWorldTransformSuite, 1, &fsp_shim_world_transform_suite},
    {kPFFillMatteSuite, 1, &fsp_shim_fill_matte_suite},
    {kPFFillMatteSuite, 2, &fsp_shim_fill_matte_suite},
    {kPFHandleSuite, 1, &fsp_shim_handle_suite},
    {kPFANSISuite, 1, &fsp_shim_ansi_suite},
    {kPFParamUtilsSuite, 1, &fsp_shim_param_utils_suite},
    {kPFParamUtilsSuite, 3, &fsp_shim_param_utils_suite},
    {kPFPathQuerySuite, 1, &fsp_shim_path_query_suite},
    {kPFPathDataSuite, 1, &fsp_shim_path_data_suite},
};

SPErr AcquireSuite(const char* name, A_long version, const void** suite) {
  const bool is_bad = name == nullptr || suite == nullptr;
  if (is_bad) {
    return fsp_shim_fail("AcquireSuite: null name or result");
  }
  for (const NamedSuite& entry : kSuites) {
    const bool is_it =
        entry.version == version && std::strcmp(entry.name, name) == 0;
    if (is_it) {
      *suite = entry.suite;
      return PF_Err_NONE;
    }
  }
  *suite = nullptr;
  const std::string what = std::string("suite \"") + name + "\" version " +
                           std::to_string(version);
  return fsp_shim_unsupported(what.c_str());
}

SPErr ReleaseSuite(const char*, A_long) { return PF_Err_NONE; }

A_Boolean IsEqual(const char* a, const char* b) {
  const bool is_same = a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
  return is_same ? 1 : 0;
}

SPErr AllocateBlock(std::size_t, void**) {
  return fsp_shim_unsupported("SPBasicSuite AllocateBlock");
}

SPErr FreeBlock(void*) {
  return fsp_shim_unsupported("SPBasicSuite FreeBlock");
}

SPErr ReallocateBlock(void*, std::size_t, void**) {
  return fsp_shim_unsupported("SPBasicSuite ReallocateBlock");
}

SPErr Undefined() { return fsp_shim_unsupported("SPBasicSuite Undefined"); }

// ---------------------------------------------------------------------
// AEGP: register and render queue state answer; the rest needs an
// After Effects project and fails loudly.
A_Err RegisterWithAegp(AEGP_GlobalRefcon, const A_char*,
                       AEGP_PluginID* plugin_id) {
  const bool has_out = plugin_id != nullptr;
  if (has_out) {
    *plugin_id = 1;
  }
  return PF_Err_NONE;
}
A_Err ExecuteScript(AEGP_PluginID, const A_char*, const A_Boolean,
                    AEGP_MemHandle*, AEGP_MemHandle*) {
  return fsp_shim_unsupported("AEGP_ExecuteScript");
}
A_Err ReportInfo(AEGP_PluginID, const A_char* info) {
  std::fprintf(stderr, "fsp: plugin says: %s\n",
               info != nullptr ? info : "");
  return PF_Err_NONE;
}
A_Err GetRenderQueueState(AEGP_RenderQueueState* state) {
  const bool has_out = state != nullptr;
  if (has_out) {
    *state = AEGP_RenderQueueState_STOPPED;
  }
  return PF_Err_NONE;
}
A_Err GetEffectLayer(PF_ProgPtr, AEGP_LayerH*) {
  return fsp_shim_unsupported("AEGP_GetEffectLayer");
}
A_Err GetNewEffectForEffect(AEGP_PluginID, PF_ProgPtr, AEGP_EffectRefH*) {
  return fsp_shim_unsupported("AEGP_GetNewEffectForEffect");
}
A_Err DisposeEffect(AEGP_EffectRefH) {
  return fsp_shim_unsupported("AEGP_DisposeEffect");
}
A_Err DisposeStream(AEGP_StreamRefH) {
  return fsp_shim_unsupported("AEGP_DisposeStream");
}
A_Err GetNewEffectStreamByIndex(AEGP_PluginID, AEGP_EffectRefH,
                                PF_ParamIndex, AEGP_StreamRefH*) {
  return fsp_shim_unsupported("AEGP_GetNewEffectStreamByIndex");
}
A_Err SetDynamicStreamFlag(AEGP_StreamRefH, AEGP_DynStreamFlags, A_Boolean,
                           A_Boolean) {
  return fsp_shim_unsupported("AEGP_SetDynamicStreamFlag");
}

// ---------------------------------------------------------------------
// Interact callbacks: parameters come from the effect_ref.
PF_Err CheckoutParam(PF_ProgPtr ref, PF_ParamIndex index, A_long, A_long,
                     A_u_long, PF_ParamDef* param) {
  const bool is_bad = ref == nullptr || param == nullptr || index < 0 ||
                      index >= static_cast<A_long>(ref->params.size());
  if (is_bad) {
    return fsp_shim_fail("checkout_param: no such parameter");
  }
  *param = ref->params[static_cast<std::size_t>(index)];
  return PF_Err_NONE;
}

PF_Err CheckinParam(PF_ProgPtr, PF_ParamDef*) { return PF_Err_NONE; }

PF_Err AddParam(PF_ProgPtr ref, PF_ParamIndex, PF_ParamDefPtr def) {
  const bool is_bad = ref == nullptr || def == nullptr;
  if (is_bad) {
    return fsp_shim_fail("add_param: null effect_ref or definition");
  }
  const bool is_recording = ref->is_recording;
  if (!is_recording) {
    return fsp_shim_fail("add_param outside PARAMS_SETUP");
  }
  // The popup entries are copied out; the plugin's string lives in its
  // own code, but a copy keeps the record whole on its own.
  FspShimRecordedParam recorded{*def, std::string()};
  const bool is_popup = def->param_type == PF_Param_POPUP &&
                        def->u.pd.u.namesptr != nullptr;
  if (is_popup) {
    recorded.items = def->u.pd.u.namesptr;
  }
  ref->recorded.push_back(recorded);
  return PF_Err_NONE;
}

PF_Err Abort(PF_ProgPtr) { return PF_Err_NONE; }
PF_Err Progress(PF_ProgPtr, A_long, A_long) { return PF_Err_NONE; }
PF_Err RegisterUi(PF_ProgPtr, void*) {
  return fsp_shim_unsupported("custom effect UI (register_ui)");
}
PF_Err CheckoutLayerAudio(PF_ProgPtr, PF_ParamIndex, A_long, A_long,
                          A_u_long, A_long, A_long, A_long, void*) {
  return fsp_shim_unsupported("audio (checkout_layer_audio)");
}
PF_Err CheckinLayerAudio(PF_ProgPtr, void*) {
  return fsp_shim_unsupported("audio (checkin_layer_audio)");
}
PF_Err GetAudioData(PF_ProgPtr, void*, void**, void*, void*, void*,
                    void*) {
  return fsp_shim_unsupported("audio (get_audio_data)");
}

}  // namespace

const PF_ANSICallbacks fsp_shim_ansi_suite = {
    Atan, Atan2, Ceil, Cos, Exp, Fabs, Floor, Fmod, Hypot, Log,
    Log10, Pow, Sin, Sqrt, Tan, Sprintf, Strcpy, Asin, Acos};
const SPBasicSuite fsp_shim_basic_suite = {
    AcquireSuite, ReleaseSuite, IsEqual, AllocateBlock,
    FreeBlock, ReallocateBlock, Undefined};
const AEGP_UtilitySuite5 fsp_shim_utility_suite = {
    RegisterWithAegp, ExecuteScript, ReportInfo};
const AEGP_RenderQueueSuite1 fsp_shim_render_queue_suite = {
    GetRenderQueueState};
const AEGP_PFInterfaceSuite1 fsp_shim_pf_interface_suite = {
    GetEffectLayer, GetNewEffectForEffect};
const AEGP_EffectSuite2 fsp_shim_effect_suite = {DisposeEffect};
const AEGP_StreamSuite2 fsp_shim_stream_suite = {
    DisposeStream, GetNewEffectStreamByIndex};
const AEGP_DynamicStreamSuite2 fsp_shim_dynamic_stream_suite = {
    SetDynamicStreamFlag};

PF_Err AEFX_AcquireSuite(PF_InData* in_data, PF_OutData*, const char* name,
                         A_long version, const char* error_string,
                         void** suite) {
  const bool is_bad = in_data == nullptr || suite == nullptr;
  if (is_bad) {
    return fsp_shim_fail("AEFX_AcquireSuite: null in_data or result");
  }
  const void* found = nullptr;
  const SPErr err = in_data->pica_basicP->AcquireSuite(name, version, &found);
  const bool is_missing = err != PF_Err_NONE || found == nullptr;
  if (is_missing) {
    *suite = nullptr;
    return fsp_shim_fail(error_string != nullptr ? error_string : name);
  }
  *suite = const_cast<void*>(found);
  return PF_Err_NONE;
}

PF_Err AEFX_ReleaseSuite(PF_InData*, PF_OutData*, const char*, A_long,
                         const char*) {
  return PF_Err_NONE;
}

namespace fsp_shim {

PF_InteractCallbacks InteractCallbacks() {
  PF_InteractCallbacks inter{};
  inter.checkout_param = CheckoutParam;
  inter.checkin_param = CheckinParam;
  inter.add_param = AddParam;
  inter.abort = Abort;
  inter.progress = Progress;
  inter.register_ui = RegisterUi;
  inter.checkout_layer_audio = CheckoutLayerAudio;
  inter.checkin_layer_audio = CheckinLayerAudio;
  inter.get_audio_data = GetAudioData;
  return inter;
}

}  // namespace fsp_shim
