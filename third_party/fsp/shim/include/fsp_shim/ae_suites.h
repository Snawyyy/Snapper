// Snapper's stand-in for the After Effects effect API: the PF suites a
// plugin asks the host for (pixels, worlds, handles, parameters).
// Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_SUITES_H_
#define FSP_SHIM_AE_SUITES_H_

#include "fsp_shim/ae_callbacks.h"

// ---------------------------------------------------------------------
// Suite names and versions, as plugins pass them to AcquireSuite. The
// names are Snapper's own; only the identifiers matter to plugin code.
#define kPFIterate8Suite "fsp Iterate8 Suite"
#define kPFIterate8SuiteVersion1 1
#define kPFIterate8SuiteVersion2 2
#define kPFIterate16Suite "fsp Iterate16 Suite"
#define kPFIterate16SuiteVersion1 1
#define kPFIterate16SuiteVersion2 2
#define kPFIterateFloatSuite "fsp IterateFloat Suite"
#define kPFIterateFloatSuiteVersion1 1
#define kPFIterateFloatSuiteVersion2 2
#define kPFWorldSuite "fsp World Suite"
#define kPFWorldSuiteVersion1 1
#define kPFWorldSuiteVersion2 2
#define kPFWorldTransformSuite "fsp World Transform Suite"
#define kPFWorldTransformSuiteVersion1 1
#define kPFFillMatteSuite "fsp Fill Matte Suite"
#define kPFFillMatteSuiteVersion1 1
#define kPFFillMatteSuiteVersion2 2
#define kPFHandleSuite "fsp Handle Suite"
#define kPFHandleSuiteVersion1 1
#define kPFANSISuite "fsp ANSI Suite"
#define kPFANSISuiteVersion1 1
#define kPFParamUtilsSuite "fsp Param Utils Suite"
#define kPFParamUtilsSuiteVersion1 1
#define kPFParamUtilsSuiteVersion3 3
#define kPFPathQuerySuite "fsp Path Query Suite"
#define kPFPathQuerySuiteVersion1 1
#define kPFPathDataSuite "fsp Path Data Suite"
#define kPFPathDataSuiteVersion1 1

// ---------------------------------------------------------------------
// Iterate suites. Version 1 and 2 have the same calls here.
typedef struct PF_Iterate8Suite1 {
  PF_Err (*iterate)(PF_InData* in_data, A_long progress_base,
                    A_long progress_final, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst);
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
  PF_Err (*iterate_origin_non_clip_src)(
      PF_InData* in_data, A_long progress_base, A_long progress_final,
      PF_EffectWorld* src, const PF_Rect* area, const PF_Point* origin,
      void* refcon, PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_generic)(A_long iterations, void* refcon,
                            PF_IterateGenericFunc fn_func);
} PF_Iterate8Suite1;
typedef PF_Iterate8Suite1 PF_Iterate8Suite2;

typedef struct PF_Iterate16Suite1 {
  PF_Err (*iterate)(PF_InData* in_data, A_long progress_base,
                    A_long progress_final, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixel16Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_origin)(PF_InData* in_data, A_long progress_base,
                           A_long progress_final, PF_EffectWorld* src,
                           const PF_Rect* area, const PF_Point* origin,
                           void* refcon, PF_IteratePixel16Func pix_fn,
                           PF_EffectWorld* dst);
  PF_Err (*iterate_origin_non_clip_src)(
      PF_InData* in_data, A_long progress_base, A_long progress_final,
      PF_EffectWorld* src, const PF_Rect* area, const PF_Point* origin,
      void* refcon, PF_IteratePixel16Func pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_generic)(A_long iterations, void* refcon,
                            PF_IterateGenericFunc fn_func);
} PF_Iterate16Suite1;
typedef PF_Iterate16Suite1 PF_Iterate16Suite2;

typedef struct PF_IterateFloatSuite1 {
  PF_Err (*iterate)(PF_InData* in_data, A_long progress_base,
                    A_long progress_final, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixelFloatFunc pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_origin)(PF_InData* in_data, A_long progress_base,
                           A_long progress_final, PF_EffectWorld* src,
                           const PF_Rect* area, const PF_Point* origin,
                           void* refcon, PF_IteratePixelFloatFunc pix_fn,
                           PF_EffectWorld* dst);
  PF_Err (*iterate_origin_non_clip_src)(
      PF_InData* in_data, A_long progress_base, A_long progress_final,
      PF_EffectWorld* src, const PF_Rect* area, const PF_Point* origin,
      void* refcon, PF_IteratePixelFloatFunc pix_fn, PF_EffectWorld* dst);
  PF_Err (*iterate_generic)(A_long iterations, void* refcon,
                            PF_IterateGenericFunc fn_func);
} PF_IterateFloatSuite1;
typedef PF_IterateFloatSuite1 PF_IterateFloatSuite2;

// ---------------------------------------------------------------------
// Worlds.
typedef struct PF_WorldSuite1 {
  PF_Err (*new_world)(PF_ProgPtr, A_long width, A_long height,
                      PF_NewWorldFlags flags, PF_EffectWorld* world);
  PF_Err (*dispose_world)(PF_ProgPtr, PF_EffectWorld* world);
  PF_Err (*get_pixel_format)(const PF_EffectWorld* world,
                             PF_PixelFormat* format);
} PF_WorldSuite1;

typedef struct PF_WorldSuite2 {
  PF_Err (*PF_NewWorld)(PF_ProgPtr, A_long width, A_long height,
                        PF_Boolean clear_pix, PF_PixelFormat format,
                        PF_EffectWorld* world);
  PF_Err (*PF_DisposeWorld)(PF_ProgPtr, PF_EffectWorld* world);
  PF_Err (*PF_GetPixelFormat)(const PF_EffectWorld* world,
                              PF_PixelFormat* format);
} PF_WorldSuite2;

typedef struct PF_WorldTransformSuite1 {
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
  PF_Err (*copy_hq)(PF_ProgPtr, PF_EffectWorld* src, PF_EffectWorld* dst,
                    PF_Rect* src_r, PF_Rect* dst_r);
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
} PF_WorldTransformSuite1;

typedef struct PF_FillMatteSuite2 {
  PF_Err (*fill)(PF_ProgPtr, const PF_Pixel* color, const PF_Rect* dst_rect,
                 PF_EffectWorld* world);
  PF_Err (*fill16)(PF_ProgPtr, const PF_Pixel16* color,
                   const PF_Rect* dst_rect, PF_EffectWorld* world);
  PF_Err (*fill_float)(PF_ProgPtr, const PF_PixelFloat* color,
                       const PF_Rect* dst_rect, PF_EffectWorld* world);
  PF_Err (*premultiply)(PF_ProgPtr, A_long forward, PF_EffectWorld* dst);
  PF_Err (*premultiply_color)(PF_ProgPtr, PF_EffectWorld* src,
                              const PF_Pixel* color, A_long forward,
                              PF_EffectWorld* dst);
  PF_Err (*premultiply_color16)(PF_ProgPtr, PF_EffectWorld* src,
                                const PF_Pixel16* color, A_long forward,
                                PF_EffectWorld* dst);
  PF_Err (*premultiply_color_float)(PF_ProgPtr, PF_EffectWorld* src,
                                    const PF_PixelFloat* color,
                                    A_long forward, PF_EffectWorld* dst);
} PF_FillMatteSuite2;
typedef PF_FillMatteSuite2 PF_FillMatteSuite1;

typedef struct PF_HandleSuite1 {
  PF_Handle (*host_new_handle)(A_u_long size);
  void* (*host_lock_handle)(PF_Handle);
  void (*host_unlock_handle)(PF_Handle);
  void (*host_dispose_handle)(PF_Handle);
  A_u_long (*host_get_handle_size)(PF_Handle);
  PF_Err (*host_resize_handle)(A_u_long new_size, PF_Handle* handle);
} PF_HandleSuite1;

typedef PF_ANSICallbacks PF_ANSICallbacksSuite1;

typedef struct PF_ParamUtilsSuite3 {
  PF_Err (*PF_UpdateParamUI)(PF_ProgPtr, PF_ParamIndex index,
                             const PF_ParamDef* param);
} PF_ParamUtilsSuite3;
typedef PF_ParamUtilsSuite3 PF_ParamUtilsSuite1;

typedef struct {
  PF_FpLong x, y;
  PF_FpLong tan_in_x, tan_in_y;
  PF_FpLong tan_out_x, tan_out_y;
} PF_PathVertex;

typedef struct PF_PathQuerySuite1 {
  PF_Err (*PF_NumPaths)(PF_ProgPtr, A_long* num_paths);
  PF_Err (*PF_PathInfo)(PF_ProgPtr, A_long index, PF_PathID* path_id);
  PF_Err (*PF_CheckoutPath)(PF_ProgPtr, PF_PathID path_id,
                            A_long what_time, A_long time_step,
                            A_u_long time_scale, PF_PathOutlinePtr* path);
  PF_Err (*PF_CheckinPath)(PF_ProgPtr, PF_PathID path_id,
                           PF_Boolean changed, PF_PathOutlinePtr path);
} PF_PathQuerySuite1;

typedef struct PF_PathDataSuite1 {
  PF_Err (*PF_PathIsOpen)(PF_ProgPtr, PF_PathOutlinePtr path,
                          PF_Boolean* open);
  PF_Err (*PF_PathNumSegments)(PF_ProgPtr, PF_PathOutlinePtr path,
                               A_long* num_segments);
  PF_Err (*PF_PathVertexInfo)(PF_ProgPtr, PF_PathOutlinePtr path,
                              A_long which, PF_PathVertex* vertex);
} PF_PathDataSuite1;

#endif  // FSP_SHIM_AE_SUITES_H_
