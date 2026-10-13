// Snapper's stand-in for the After Effects effect API: the AEGP suites
// and AEGP_SuiteHandler, which hands every suite out by accessor.
// Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_AEGP_H_
#define FSP_SHIM_AE_AEGP_H_

#include "fsp_shim/ae_suites.h"

// AEGP suites are After Effects' project-level calls. Snapper has no
// project to answer from: registering and the render queue state work,
// the rest fail loudly.
typedef void* AEGP_GlobalRefcon;
typedef struct FspShimMemHandle* AEGP_MemHandle;

typedef struct AEGP_UtilitySuite5 {
  A_Err (*AEGP_RegisterWithAEGP)(AEGP_GlobalRefcon refcon,
                                 const A_char* plugin_name,
                                 AEGP_PluginID* plugin_id);
  A_Err (*AEGP_ExecuteScript)(AEGP_PluginID plugin_id,
                              const A_char* script,
                              const A_Boolean platform_encoding,
                              AEGP_MemHandle* result,
                              AEGP_MemHandle* error);
  A_Err (*AEGP_ReportInfo)(AEGP_PluginID plugin_id, const A_char* info);
} AEGP_UtilitySuite5;
typedef AEGP_UtilitySuite5 AEGP_UtilitySuite3;
typedef AEGP_UtilitySuite5 AEGP_UtilitySuite6;

typedef struct AEGP_RenderQueueSuite1 {
  A_Err (*AEGP_GetRenderQueueState)(AEGP_RenderQueueState* state);
} AEGP_RenderQueueSuite1;

typedef struct AEGP_PFInterfaceSuite1 {
  A_Err (*AEGP_GetEffectLayer)(PF_ProgPtr effect_ref, AEGP_LayerH* layer);
  A_Err (*AEGP_GetNewEffectForEffect)(AEGP_PluginID plugin_id,
                                      PF_ProgPtr effect_ref,
                                      AEGP_EffectRefH* effect);
} AEGP_PFInterfaceSuite1;

typedef struct AEGP_EffectSuite2 {
  A_Err (*AEGP_DisposeEffect)(AEGP_EffectRefH effect);
} AEGP_EffectSuite2;
typedef AEGP_EffectSuite2 AEGP_EffectSuite3;
typedef AEGP_EffectSuite2 AEGP_EffectSuite4;

typedef struct AEGP_StreamSuite2 {
  A_Err (*AEGP_DisposeStream)(AEGP_StreamRefH stream);
  A_Err (*AEGP_GetNewEffectStreamByIndex)(AEGP_PluginID plugin_id,
                                          AEGP_EffectRefH effect,
                                          PF_ParamIndex index,
                                          AEGP_StreamRefH* stream);
} AEGP_StreamSuite2;
typedef AEGP_StreamSuite2 AEGP_StreamSuite5;

typedef struct AEGP_DynamicStreamSuite2 {
  A_Err (*AEGP_SetDynamicStreamFlag)(AEGP_StreamRefH stream,
                                     AEGP_DynStreamFlags flag,
                                     A_Boolean undoable, A_Boolean set);
} AEGP_DynamicStreamSuite2;
typedef AEGP_DynamicStreamSuite2 AEGP_DynamicStreamSuite4;

// The host's suite tables (shim/src/suites.cpp).
extern const SPBasicSuite fsp_shim_basic_suite;
extern const PF_Iterate8Suite1 fsp_shim_iterate8_suite;
extern const PF_Iterate16Suite1 fsp_shim_iterate16_suite;
extern const PF_IterateFloatSuite1 fsp_shim_iterate_float_suite;
extern const PF_WorldSuite1 fsp_shim_world_suite1;
extern const PF_WorldSuite2 fsp_shim_world_suite2;
extern const PF_WorldTransformSuite1 fsp_shim_world_transform_suite;
extern const PF_FillMatteSuite2 fsp_shim_fill_matte_suite;
extern const PF_HandleSuite1 fsp_shim_handle_suite;
extern const PF_ANSICallbacks fsp_shim_ansi_suite;
extern const PF_ParamUtilsSuite3 fsp_shim_param_utils_suite;
extern const PF_PathQuerySuite1 fsp_shim_path_query_suite;
extern const PF_PathDataSuite1 fsp_shim_path_data_suite;
extern const AEGP_UtilitySuite5 fsp_shim_utility_suite;
extern const AEGP_RenderQueueSuite1 fsp_shim_render_queue_suite;
extern const AEGP_PFInterfaceSuite1 fsp_shim_pf_interface_suite;
extern const AEGP_EffectSuite2 fsp_shim_effect_suite;
extern const AEGP_StreamSuite2 fsp_shim_stream_suite;
extern const AEGP_DynamicStreamSuite2 fsp_shim_dynamic_stream_suite;

// Hands out the suites by accessor, as plugins use it.
class AEGP_SuiteHandler {
 public:
  explicit AEGP_SuiteHandler(const SPBasicSuite* basic) : basic_(basic) {}
  const SPBasicSuite* Pica() const { return basic_; }

  const PF_ANSICallbacksSuite1* ANSICallbacksSuite1() const {
    return &fsp_shim_ansi_suite;
  }
  const PF_HandleSuite1* HandleSuite1() const {
    return &fsp_shim_handle_suite;
  }
  const PF_Iterate8Suite1* Iterate8Suite1() const {
    return &fsp_shim_iterate8_suite;
  }
  const PF_Iterate8Suite2* Iterate8Suite2() const {
    return &fsp_shim_iterate8_suite;
  }
  const PF_Iterate16Suite1* Iterate16Suite1() const {
    return &fsp_shim_iterate16_suite;
  }
  const PF_Iterate16Suite2* Iterate16Suite2() const {
    return &fsp_shim_iterate16_suite;
  }
  const PF_IterateFloatSuite1* IterateFloatSuite1() const {
    return &fsp_shim_iterate_float_suite;
  }
  const PF_IterateFloatSuite2* IterateFloatSuite2() const {
    return &fsp_shim_iterate_float_suite;
  }
  const PF_WorldSuite1* WorldSuite1() const {
    return &fsp_shim_world_suite1;
  }
  const PF_WorldSuite2* WorldSuite2() const {
    return &fsp_shim_world_suite2;
  }
  const PF_WorldTransformSuite1* WorldTransformSuite1() const {
    return &fsp_shim_world_transform_suite;
  }
  const PF_FillMatteSuite1* FillMatteSuite1() const {
    return &fsp_shim_fill_matte_suite;
  }
  const PF_FillMatteSuite2* FillMatteSuite2() const {
    return &fsp_shim_fill_matte_suite;
  }
  const PF_ParamUtilsSuite1* ParamUtilsSuite1() const {
    return &fsp_shim_param_utils_suite;
  }
  const PF_ParamUtilsSuite3* ParamUtilsSuite3() const {
    return &fsp_shim_param_utils_suite;
  }
  const AEGP_UtilitySuite3* UtilitySuite3() const {
    return &fsp_shim_utility_suite;
  }
  const AEGP_UtilitySuite5* UtilitySuite5() const {
    return &fsp_shim_utility_suite;
  }
  const AEGP_UtilitySuite6* UtilitySuite6() const {
    return &fsp_shim_utility_suite;
  }
  const AEGP_RenderQueueSuite1* RenderQueueSuite1() const {
    return &fsp_shim_render_queue_suite;
  }
  const AEGP_PFInterfaceSuite1* PFInterfaceSuite1() const {
    return &fsp_shim_pf_interface_suite;
  }
  const AEGP_EffectSuite2* EffectSuite2() const {
    return &fsp_shim_effect_suite;
  }
  const AEGP_EffectSuite3* EffectSuite3() const {
    return &fsp_shim_effect_suite;
  }
  const AEGP_EffectSuite4* EffectSuite4() const {
    return &fsp_shim_effect_suite;
  }
  const AEGP_StreamSuite2* StreamSuite2() const {
    return &fsp_shim_stream_suite;
  }
  const AEGP_StreamSuite5* StreamSuite5() const {
    return &fsp_shim_stream_suite;
  }
  const AEGP_DynamicStreamSuite2* DynamicStreamSuite2() const {
    return &fsp_shim_dynamic_stream_suite;
  }
  const AEGP_DynamicStreamSuite4* DynamicStreamSuite4() const {
    return &fsp_shim_dynamic_stream_suite;
  }

 private:
  const SPBasicSuite* basic_;
};

#endif  // FSP_SHIM_AE_AEGP_H_
