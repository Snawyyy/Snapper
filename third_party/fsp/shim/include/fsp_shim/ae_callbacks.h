// Snapper's stand-in for the After Effects effect API: the host
// callbacks in in_data, in_data and out_data themselves, the smart render
// records and plugin registration. Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_CALLBACKS_H_
#define FSP_SHIM_AE_CALLBACKS_H_

#include <cstddef>

#include "fsp_shim/ae_effect.h"

typedef A_long SPErr;
typedef A_long PF_InFlags;
typedef A_long PF_OutFlags;
typedef A_long PF_OutFlags2;
typedef struct PF_InData PF_InData;

// Per-pixel and per-index work functions handed to the iterate calls.
typedef PF_Err (*PF_IteratePixel8Func)(void* refcon, A_long x, A_long y,
                                       PF_Pixel* in, PF_Pixel* out);
typedef PF_Err (*PF_IteratePixel16Func)(void* refcon, A_long x, A_long y,
                                        PF_Pixel16* in, PF_Pixel16* out);
typedef PF_Err (*PF_IteratePixelFloatFunc)(void* refcon, A_long x,
                                           A_long y, PF_PixelFloat* in,
                                           PF_PixelFloat* out);
typedef PF_Err (*PF_IterateGenericFunc)(void* refcon, A_long thread_index,
                                        A_long i, A_long iterations);
typedef PF_Err (*PF_IteratePixelLUTFunc)(void* refcon, A_long x, A_long y,
                                         PF_Pixel* in, PF_Pixel* out);

// The host's basic suite lookup (in_data->pica_basicP).
typedef struct SPBasicSuite {
  SPErr (*AcquireSuite)(const char* name, A_long version,
                        const void** suite);
  SPErr (*ReleaseSuite)(const char* name, A_long version);
  A_Boolean (*IsEqual)(const char* token1, const char* token2);
  SPErr (*AllocateBlock)(std::size_t size, void** block);
  SPErr (*FreeBlock)(void* block);
  SPErr (*ReallocateBlock)(void* block, std::size_t new_size,
                           void** new_block);
  SPErr (*Undefined)(void);
} SPBasicSuite;

// The C library calls After Effects hands out (utils->ansi).
typedef struct PF_ANSICallbacks {
  A_FpLong (*atan)(A_FpLong);
  A_FpLong (*atan2)(A_FpLong y, A_FpLong x);
  A_FpLong (*ceil)(A_FpLong);
  A_FpLong (*cos)(A_FpLong);
  A_FpLong (*exp)(A_FpLong);
  A_FpLong (*fabs)(A_FpLong);
  A_FpLong (*floor)(A_FpLong);
  A_FpLong (*fmod)(A_FpLong x, A_FpLong y);
  A_FpLong (*hypot)(A_FpLong x, A_FpLong y);
  A_FpLong (*log)(A_FpLong);
  A_FpLong (*log10)(A_FpLong);
  A_FpLong (*pow)(A_FpLong x, A_FpLong y);
  A_FpLong (*sin)(A_FpLong);
  A_FpLong (*sqrt)(A_FpLong);
  A_FpLong (*tan)(A_FpLong);
  int (*sprintf)(A_char*, const A_char*, ...);
  A_char* (*strcpy)(A_char*, const A_char*);
  A_FpLong (*asin)(A_FpLong);
  A_FpLong (*acos)(A_FpLong);
} PF_ANSICallbacks;

typedef struct PF_SampPB {
  PF_Fixed x_radius, y_radius;
  PF_Fixed area;
  PF_EffectWorld* src;
  A_long samp_behave;
  A_long allow_asynch;
  A_long motion_blur;
  PF_CompositeMatrix* mb_matrix;
  A_long mb_samples;
  A_long reserved[8];
} PF_SampPB;

// in_data->utils. Calls Snapper does not carry yet fail loudly.
typedef struct PF_UtilCallbacks {
  PF_Err (*begin_sampling)(PF_ProgPtr, PF_Quality, PF_ModeFlags,
                           PF_SampPB*);
  PF_Err (*subpixel_sample)(PF_ProgPtr, PF_Fixed x, PF_Fixed y,
                            const PF_SampPB*, PF_Pixel* dst);
  PF_Err (*area_sample)(PF_ProgPtr, PF_Fixed x, PF_Fixed y,
                        const PF_SampPB*, PF_Pixel* dst);
  void* get_batch_func;
  PF_Err (*end_sampling)(PF_ProgPtr, PF_Quality, PF_ModeFlags,
                         PF_SampPB*);
  PF_Err (*composite_rect)(PF_ProgPtr, PF_Rect* src_rect,
                           A_long src_opacity, PF_EffectWorld* src,
                           A_long dest_x, A_long dest_y, A_long field,
                           A_long transfer_mode, PF_EffectWorld* dst);
  PF_Err (*blend)(PF_ProgPtr, const PF_EffectWorld* src1,
                  const PF_EffectWorld* src2, PF_Fixed ratio,
                  PF_EffectWorld* dst);
  PF_Err (*convolve)(PF_ProgPtr, PF_EffectWorld* src, const PF_Rect* area,
                     A_long flags, A_long kernel_size, void* a_kernel,
                     void* r_kernel, void* g_kernel, void* b_kernel,
                     PF_EffectWorld* dst);
  PF_Err (*copy)(PF_ProgPtr, PF_EffectWorld* src, PF_EffectWorld* dst,
                 PF_Rect* src_r, PF_Rect* dst_r);
  PF_Err (*fill)(PF_ProgPtr, const PF_Pixel* color, const PF_Rect* dst_rect,
                 PF_EffectWorld* world);
  PF_Err (*gaussian_kernel)(PF_ProgPtr, A_FpLong k_radius, A_long flags,
                            A_FpLong multiplier, A_long* diameter,
                            void* kernel);
  PF_Err (*iterate)(PF_InData* in_data, A_long progress_base,
                    A_long progress_final, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*premultiply)(PF_ProgPtr, A_long forward, PF_EffectWorld* dst);
  PF_Err (*premultiply_color)(PF_ProgPtr, PF_EffectWorld* src,
                              const PF_Pixel* color, A_long forward,
                              PF_EffectWorld* dst);
  PF_Err (*new_world)(PF_ProgPtr, A_long width, A_long height,
                      PF_NewWorldFlags flags, PF_EffectWorld* world);
  PF_Err (*dispose_world)(PF_ProgPtr, PF_EffectWorld* world);
  PF_Err (*iterate_origin)(PF_InData* in_data, A_long progress_base,
                           A_long progress_final, PF_EffectWorld* src,
                           const PF_Rect* area, const PF_Point* origin,
                           void* refcon, PF_IteratePixel8Func pix_fn,
                           PF_EffectWorld* dst);
  PF_Err (*iterate_lut)(PF_InData* in_data, A_long progress_base,
                        A_long progress_final, PF_EffectWorld* src,
                        const PF_Rect* area, A_u_char* a_lut0,
                        A_u_char* r_lut0, A_u_char* g_lut0,
                        A_u_char* b_lut0, PF_EffectWorld* dst);
  PF_Err (*transfer_rect)(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_Field,
                          const PF_Rect* src_rec, const PF_EffectWorld* src,
                          const void* comp_mode, const void* mask_world,
                          A_long dest_x, A_long dest_y,
                          PF_EffectWorld* dst);
  PF_Err (*transform_world)(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_Field,
                            const PF_EffectWorld* src,
                            const void* comp_mode, const void* mask_world,
                            const PF_FloatMatrix* matrices,
                            A_long num_matrices, PF_Boolean src2dst_matrix,
                            const PF_Rect* dest_rect, PF_EffectWorld* dst);
  PF_Handle (*host_new_handle)(A_u_long size);
  void* (*host_lock_handle)(PF_Handle);
  void (*host_unlock_handle)(PF_Handle);
  void (*host_dispose_handle)(PF_Handle);
  PF_Err (*get_callback_addr)(PF_ProgPtr, PF_Quality, PF_ModeFlags,
                              A_long which_callback, void** fn_ptr);
  PF_Err (*app)(PF_ProgPtr, A_long, ...);
  PF_ANSICallbacks ansi;
  void* colorCB;
  PF_Err (*get_platform_data)(PF_ProgPtr, A_long which, void* data);
  A_u_long (*host_get_handle_size)(PF_Handle);
  PF_Err (*iterate_origin_non_clip_src)(
      PF_InData* in_data, A_long progress_base, A_long progress_final,
      PF_EffectWorld* src, const PF_Rect* area, const PF_Point* origin,
      void* refcon, PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_generic)(A_long iterations, void* refcon,
                            PF_IterateGenericFunc fn_func);
  PF_Err (*host_resize_handle)(A_u_long new_sizeL, PF_Handle* handle);
  PF_Err (*subpixel_sample16)(PF_ProgPtr, PF_Fixed x, PF_Fixed y,
                              const PF_SampPB*, PF_Pixel16* dst);
  PF_Err (*area_sample16)(PF_ProgPtr, PF_Fixed x, PF_Fixed y,
                          const PF_SampPB*, PF_Pixel16* dst);
  PF_Err (*fill16)(PF_ProgPtr, const PF_Pixel16* color,
                   const PF_Rect* dst_rect, PF_EffectWorld* world);
  PF_Err (*premultiply_color16)(PF_ProgPtr, PF_EffectWorld* src,
                                const PF_Pixel16* color, A_long forward,
                                PF_EffectWorld* dst);
  PF_Err (*iterate16)(PF_InData* in_data, A_long progress_base,
                      A_long progress_final, PF_EffectWorld* src,
                      const PF_Rect* area, void* refcon,
                      PF_IteratePixel16Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_origin16)(PF_InData* in_data, A_long progress_base,
                             A_long progress_final, PF_EffectWorld* src,
                             const PF_Rect* area, const PF_Point* origin,
                             void* refcon, PF_IteratePixel16Func pix_fn,
                             PF_EffectWorld* dst);
} PF_UtilCallbacks;

// in_data->inter: parameter and progress calls.
typedef struct PF_InteractCallbacks {
  PF_Err (*checkout_param)(PF_ProgPtr, PF_ParamIndex index,
                           A_long what_time, A_long time_step,
                           A_u_long time_scale, PF_ParamDef* param);
  PF_Err (*checkin_param)(PF_ProgPtr, PF_ParamDef* param);
  PF_Err (*add_param)(PF_ProgPtr, PF_ParamIndex index, PF_ParamDefPtr def);
  PF_Err (*abort)(PF_ProgPtr);
  PF_Err (*progress)(PF_ProgPtr, A_long current, A_long total);
  PF_Err (*register_ui)(PF_ProgPtr, void* custom_ui_info);
  PF_Err (*checkout_layer_audio)(PF_ProgPtr, PF_ParamIndex, A_long,
                                 A_long, A_u_long, A_long, A_long, A_long,
                                 void*);
  PF_Err (*checkin_layer_audio)(PF_ProgPtr, void*);
  PF_Err (*get_audio_data)(PF_ProgPtr, void*, void**, void*, void*,
                           void*, void*);
  void* reserved_str[3];
  void* reserved[10];
} PF_InteractCallbacks;

typedef struct { A_short major, minor; } PF_SpecVersion;

typedef struct PF_InData {
  PF_InteractCallbacks inter;
  PF_UtilCallbacks* utils;
  PF_ProgPtr effect_ref;
  PF_Quality quality;
  PF_SpecVersion version;
  A_long serial_num;
  A_long appl_id;
  A_long num_params;
  A_long reserved;
  A_long what_cpu;
  A_long what_fpu;
  A_long current_time;
  A_long time_step;
  A_long total_time;
  A_long local_time_step;
  A_u_long time_scale;
  PF_Field field;
  PF_Fixed shutter_angle;
  A_long width;
  A_long height;
  PF_Rect extent_hint;
  A_long output_origin_x;
  A_long output_origin_y;
  PF_RationalScale downsample_x;
  PF_RationalScale downsample_y;
  PF_RationalScale pixel_aspect_ratio;
  PF_InFlags in_flags;
  PF_Handle global_data;
  PF_Handle sequence_data;
  PF_Handle frame_data;
  A_long start_sampL;
  A_long dur_sampL;
  A_long total_sampL;
  void* src_snd;
  SPBasicSuite* pica_basicP;
  A_long pre_effect_source_origin_x;
  A_long pre_effect_source_origin_y;
  PF_Fixed shutter_phase;
} PF_InData;

typedef struct PF_OutData {
  A_u_long my_version;
  A_char name[32];
  PF_Handle global_data;
  A_long num_params;
  PF_Handle sequence_data;
  A_long flat_sdata_size;
  PF_Handle frame_data;
  A_long width;
  A_long height;
  PF_Point origin;
  PF_OutFlags out_flags;
  A_char return_msg[256];
  A_long start_sampL;
  A_long dur_sampL;
  void* dest_snd;
  PF_OutFlags2 out_flags2;
} PF_OutData;

// ---------------------------------------------------------------------
// Smart render records. Snapper never sends the smart commands; the
// records exist so the plugins' smart paths compile.
typedef struct {
  PF_LRect rect;
  PF_Field field;
  PF_ChannelMask channel_mask;
  PF_Boolean preserve_rgb_of_zero_alpha;
  char unused[3];
  A_long reserved[4];
} PF_RenderRequest;

typedef struct {
  PF_LRect result_rect;
  PF_LRect max_result_rect;
  PF_RationalScale par;
  A_long solid;
  PF_Boolean reservedB[3];
  A_long ref_width;
  A_long ref_height;
  A_long reserved[6];
} PF_CheckoutResult;

typedef struct {
  PF_RenderRequest output_request;
  short bitdepth;
  const void* gpu_data;
  A_long what_gpu;
  A_u_long device_index;
} PF_PreRenderInput;

typedef void (*PF_DeletePreRenderDataFunc)(void* pre_render_data);

typedef struct {
  PF_LRect result_rect;
  PF_LRect max_result_rect;
  PF_Boolean solid;
  PF_Boolean reserved;
  A_long flags;
  void* pre_render_data;
  PF_DeletePreRenderDataFunc delete_pre_render_data_func;
} PF_PreRenderOutput;

typedef struct {
  PF_Err (*checkout_layer)(PF_ProgPtr, PF_ParamIndex index,
                           A_long checkout_id, const PF_RenderRequest* req,
                           A_long what_time, A_long time_step,
                           A_u_long time_scale,
                           PF_CheckoutResult* checkout_result);
  PF_Err (*GuidMixInPtr)(PF_ProgPtr, A_u_long buf_size, const void* buf);
} PF_PreRenderCallbacks;

typedef struct {
  PF_PreRenderInput* input;
  PF_PreRenderOutput* output;
  PF_PreRenderCallbacks* cb;
} PF_PreRenderExtra;

typedef struct {
  PF_RenderRequest output_request;
  short bitdepth;
  void* pre_render_data;
  const void* gpu_data;
  A_long what_gpu;
  A_u_long device_index;
} PF_SmartRenderInput;

typedef struct {
  PF_Err (*checkout_layer_pixels)(PF_ProgPtr, A_long checkout_id,
                                  PF_EffectWorld** pixels);
  PF_Err (*checkin_layer_pixels)(PF_ProgPtr, A_long checkout_id);
  PF_Err (*checkout_output)(PF_ProgPtr, PF_EffectWorld** output);
  PF_Err (*is_layer_pixel_data_valid)(PF_ProgPtr, A_long checkout_id,
                                      PF_Boolean* valid);
} PF_SmartRenderCallbacks;

typedef struct {
  PF_SmartRenderInput* input;
  PF_SmartRenderCallbacks* cb;
} PF_SmartRenderExtra;

// ---------------------------------------------------------------------
// Registration: the plugin's PluginDataEntryFunction2 reports its names
// through this callback.
typedef PF_Err (*PF_PluginDataCB2)(
    PF_PluginDataPtr data, const A_u_char* name, const A_u_char* match_name,
    const A_u_char* category, const A_u_char* entry_point, A_long kind,
    A_long api_major, A_long api_minor, A_long reserved,
    const A_u_char* support_url);

#define AE_RESERVED_INFO 8

// The plugin entry point's shape.
typedef PF_Err (*PF_EffectMainFunc)(PF_Cmd cmd, PF_InData* in_data,
                                    PF_OutData* out_data,
                                    PF_ParamDef* params[],
                                    PF_LayerDef* output, void* extra);
typedef PF_Err (*PF_PluginDataEntryFunc2)(PF_PluginDataPtr data,
                                          PF_PluginDataCB2 callback,
                                          SPBasicSuite* basic_suite,
                                          const char* host_name,
                                          const char* host_version);

#endif  // FSP_SHIM_AE_CALLBACKS_H_
