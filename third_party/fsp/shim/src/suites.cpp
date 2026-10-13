// The suite tables the stand-in host hands out, looked up by name in
// AcquireSuite or by accessor in AEGP_SuiteHandler.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "fsp_shim/ae_all.h"
#include "fsp_shim/iterate.h"
#include "fsp_shim/matte.h"
#include "fsp_shim/session.h"

namespace {

// ---------------------------------------------------------------------
// Worlds.
PF_Err NewWorldFlags(PF_ProgPtr ref, A_long width, A_long height,
                     PF_NewWorldFlags flags, PF_EffectWorld* world) {
  const bool is_deep = (flags & PF_NewWorldFlag_DEEP_PIXELS) != 0;
  return fsp_shim::NewWorld(
      ref, width, height,
      is_deep ? PF_PixelFormat_ARGB64 : PF_PixelFormat_ARGB32, world);
}

PF_Err NewWorldFormat(PF_ProgPtr ref, A_long width, A_long height,
                      PF_Boolean clear, PF_PixelFormat format,
                      PF_EffectWorld* world) {
  (void)clear;
  return fsp_shim::NewWorld(ref, width, height, format, world);
}

PF_Err DisposeWorld(PF_ProgPtr ref, PF_EffectWorld* world) {
  return fsp_shim::DisposeWorld(ref, world);
}

PF_Err GetPixelFormat(const PF_EffectWorld* world, PF_PixelFormat* format) {
  const bool is_bad = world == nullptr || format == nullptr;
  if (is_bad) {
    return fsp_shim_fail("get pixel format: null world");
  }
  *format = fsp_shim::WorldFormat(world);
  return PF_Err_NONE;
}

PF_Err Copy(PF_ProgPtr, PF_EffectWorld* src, PF_EffectWorld* dst,
            PF_Rect* src_r, PF_Rect* dst_r) {
  return fsp_shim_copy(src, dst, src_r, dst_r);
}

PF_Err Fill(PF_ProgPtr, const PF_Pixel* color, const PF_Rect* rect,
            PF_EffectWorld* world) {
  return fsp_shim_fill8(color, rect, world);
}

PF_Err Fill16(PF_ProgPtr, const PF_Pixel16* color, const PF_Rect* rect,
              PF_EffectWorld* world) {
  const bool is_clear = color == nullptr;
  if (is_clear) {
    return fsp_shim_fill8(nullptr, rect, world);
  }
  return fsp_shim_unsupported("fill16 with a color");
}

PF_Err FillFloat(PF_ProgPtr, const PF_PixelFloat* color,
                 const PF_Rect* rect, PF_EffectWorld* world) {
  const bool is_clear = color == nullptr;
  if (is_clear) {
    return fsp_shim_fill8(nullptr, rect, world);
  }
  return fsp_shim_unsupported("fill_float with a color");
}

PF_Err Blend(PF_ProgPtr, const PF_EffectWorld* a, const PF_EffectWorld* b,
             PF_Fixed ratio, PF_EffectWorld* dst) {
  return fsp_shim::Blend(a, b, ratio, dst);
}

PF_Err Premultiply(PF_ProgPtr, A_long forward, PF_EffectWorld* dst) {
  return fsp_shim::PremultiplyColor(dst, nullptr, nullptr, forward != 0,
                                    dst);
}

PF_Err PremultiplyColor(PF_ProgPtr, PF_EffectWorld* src,
                        const PF_Pixel* color, A_long forward,
                        PF_EffectWorld* dst) {
  return fsp_shim::PremultiplyColor(src, color, nullptr, forward != 0, dst);
}

PF_Err PremultiplyColor16(PF_ProgPtr, PF_EffectWorld* src,
                          const PF_Pixel16* color, A_long forward,
                          PF_EffectWorld* dst) {
  return fsp_shim::PremultiplyColor(src, nullptr, color, forward != 0, dst);
}

PF_Err PremultiplyColorFloat(PF_ProgPtr, PF_EffectWorld*,
                             const PF_PixelFloat*, A_long,
                             PF_EffectWorld*) {
  return fsp_shim_unsupported("premultiply_color_float");
}

// ---------------------------------------------------------------------
// Calls the stand-in does not carry yet: each says so by name.
PF_Err CompositeRect(PF_ProgPtr, PF_Rect*, A_long, PF_EffectWorld*, A_long,
                     A_long, A_long, A_long, PF_EffectWorld*) {
  return fsp_shim_unsupported("composite_rect");
}
PF_Err Convolve(PF_ProgPtr, PF_EffectWorld*, const PF_Rect*, A_long,
                A_long, void*, void*, void*, void*, PF_EffectWorld*) {
  return fsp_shim_unsupported("convolve");
}
PF_Err TransferRect(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_Field,
                    const PF_Rect*, const PF_EffectWorld*, const void*,
                    const void*, A_long, A_long, PF_EffectWorld*) {
  return fsp_shim_unsupported("transfer_rect");
}
PF_Err TransformWorld(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_Field,
                      const PF_EffectWorld*, const void*, const void*,
                      const PF_FloatMatrix*, A_long, PF_Boolean,
                      const PF_Rect*, PF_EffectWorld*) {
  return fsp_shim_unsupported("transform_world");
}
PF_Err BeginSampling(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_SampPB*) {
  return fsp_shim_unsupported("begin_sampling");
}
PF_Err EndSampling(PF_ProgPtr, PF_Quality, PF_ModeFlags, PF_SampPB*) {
  return fsp_shim_unsupported("end_sampling");
}
PF_Err SubpixelSample(PF_ProgPtr, PF_Fixed, PF_Fixed, const PF_SampPB*,
                      PF_Pixel*) {
  return fsp_shim_unsupported("subpixel_sample");
}
PF_Err AreaSample(PF_ProgPtr, PF_Fixed, PF_Fixed, const PF_SampPB*,
                  PF_Pixel*) {
  return fsp_shim_unsupported("area_sample");
}
PF_Err SubpixelSample16(PF_ProgPtr, PF_Fixed, PF_Fixed, const PF_SampPB*,
                        PF_Pixel16*) {
  return fsp_shim_unsupported("subpixel_sample16");
}
PF_Err AreaSample16(PF_ProgPtr, PF_Fixed, PF_Fixed, const PF_SampPB*,
                    PF_Pixel16*) {
  return fsp_shim_unsupported("area_sample16");
}
PF_Err GaussianKernel(PF_ProgPtr, A_FpLong, A_long, A_FpLong, A_long*,
                      void*) {
  return fsp_shim_unsupported("gaussian_kernel");
}
PF_Err GetCallbackAddr(PF_ProgPtr, PF_Quality, PF_ModeFlags, A_long,
                       void**) {
  return fsp_shim_unsupported("get_callback_addr");
}
PF_Err App(PF_ProgPtr, A_long, ...) {
  return fsp_shim_unsupported("utils app");
}
PF_Err GetPlatformData(PF_ProgPtr, A_long, void*) {
  return fsp_shim_unsupported("get_platform_data");
}
PF_Err IterateLut(PF_InData*, A_long, A_long, PF_EffectWorld*,
                  const PF_Rect*, A_u_char*, A_u_char*, A_u_char*,
                  A_u_char*, PF_EffectWorld*) {
  return fsp_shim_unsupported("iterate_lut");
}
PF_Err UpdateParamUi(PF_ProgPtr, PF_ParamIndex, const PF_ParamDef*) {
  // Snapper builds its own panel from the parameters; nothing to redraw.
  return PF_Err_NONE;
}
PF_Err NumPaths(PF_ProgPtr, A_long*) {
  return fsp_shim_unsupported("masks and paths (PF_NumPaths)");
}
PF_Err PathInfo(PF_ProgPtr, A_long, PF_PathID*) {
  return fsp_shim_unsupported("masks and paths (PF_PathInfo)");
}
PF_Err CheckoutPath(PF_ProgPtr, PF_PathID, A_long, A_long, A_u_long,
                    PF_PathOutlinePtr*) {
  return fsp_shim_unsupported("masks and paths (PF_CheckoutPath)");
}
PF_Err CheckinPath(PF_ProgPtr, PF_PathID, PF_Boolean, PF_PathOutlinePtr) {
  return fsp_shim_unsupported("masks and paths (PF_CheckinPath)");
}
PF_Err PathIsOpen(PF_ProgPtr, PF_PathOutlinePtr, PF_Boolean*) {
  return fsp_shim_unsupported("masks and paths (PF_PathIsOpen)");
}
PF_Err PathNumSegments(PF_ProgPtr, PF_PathOutlinePtr, A_long*) {
  return fsp_shim_unsupported("masks and paths (PF_PathNumSegments)");
}
PF_Err PathVertexInfo(PF_ProgPtr, PF_PathOutlinePtr, A_long,
                      PF_PathVertex*) {
  return fsp_shim_unsupported("masks and paths (PF_PathVertexInfo)");
}

}  // namespace

const PF_Iterate8Suite1 fsp_shim_iterate8_suite = {
    fsp_shim::Iterate8, fsp_shim::IterateOrigin8, IterateLut,
    fsp_shim::IterateOrigin8, fsp_shim::IterateGeneric};
const PF_Iterate16Suite1 fsp_shim_iterate16_suite = {
    fsp_shim::Iterate16, fsp_shim::IterateOrigin16,
    fsp_shim::IterateOrigin16, fsp_shim::IterateGeneric};
const PF_IterateFloatSuite1 fsp_shim_iterate_float_suite = {
    fsp_shim::IterateFloat, fsp_shim::IterateOriginFloat,
    fsp_shim::IterateOriginFloat, fsp_shim::IterateGeneric};
const PF_WorldSuite1 fsp_shim_world_suite1 = {NewWorldFlags, DisposeWorld,
                                              GetPixelFormat};
const PF_WorldSuite2 fsp_shim_world_suite2 = {NewWorldFormat, DisposeWorld,
                                              GetPixelFormat};
const PF_WorldTransformSuite1 fsp_shim_world_transform_suite = {
    CompositeRect, Blend, Convolve, Copy, Copy, TransferRect,
    TransformWorld};
const PF_FillMatteSuite2 fsp_shim_fill_matte_suite = {
    Fill, Fill16, FillFloat, Premultiply, PremultiplyColor,
    PremultiplyColor16, PremultiplyColorFloat};
const PF_HandleSuite1 fsp_shim_handle_suite = {
    fsp_shim_new_handle, fsp_shim_lock_handle, fsp_shim_unlock_handle,
    fsp_shim_dispose_handle, fsp_shim_handle_size, fsp_shim_resize_handle};
const PF_ParamUtilsSuite3 fsp_shim_param_utils_suite = {UpdateParamUi};
const PF_PathQuerySuite1 fsp_shim_path_query_suite = {
    NumPaths, PathInfo, CheckoutPath, CheckinPath};
const PF_PathDataSuite1 fsp_shim_path_data_suite = {
    PathIsOpen, PathNumSegments, PathVertexInfo};

namespace fsp_shim {

// The callbacks of in_data->utils. Pointers to the host's own calls
// where it has them, loud failures for the rest.
const PF_UtilCallbacks* UtilCallbacks() {
  static const PF_UtilCallbacks utils = [] {
    PF_UtilCallbacks u{};
    u.begin_sampling = BeginSampling;
    u.subpixel_sample = SubpixelSample;
    u.area_sample = AreaSample;
    u.end_sampling = EndSampling;
    u.gaussian_kernel = GaussianKernel;
    u.get_callback_addr = GetCallbackAddr;
    u.app = App;
    u.get_platform_data = GetPlatformData;
    u.subpixel_sample16 = SubpixelSample16;
    u.area_sample16 = AreaSample16;
    u.composite_rect = CompositeRect;
    u.blend = Blend;
    u.convolve = Convolve;
    u.copy = Copy;
    u.fill = Fill;
    u.iterate = Iterate8;
    u.premultiply = Premultiply;
    u.premultiply_color = PremultiplyColor;
    u.new_world = NewWorldFlags;
    u.dispose_world = DisposeWorld;
    u.iterate_origin = IterateOrigin8;
    u.iterate_lut = IterateLut;
    u.transfer_rect = TransferRect;
    u.transform_world = TransformWorld;
    u.host_new_handle = fsp_shim_new_handle;
    u.host_lock_handle = fsp_shim_lock_handle;
    u.host_unlock_handle = fsp_shim_unlock_handle;
    u.host_dispose_handle = fsp_shim_dispose_handle;
    u.ansi = fsp_shim_ansi_suite;
    u.host_get_handle_size = fsp_shim_handle_size;
    u.iterate_origin_non_clip_src = IterateOrigin8;
    u.iterate_generic = IterateGeneric;
    u.host_resize_handle = fsp_shim_resize_handle;
    u.fill16 = Fill16;
    u.premultiply_color16 = PremultiplyColor16;
    u.iterate16 = Iterate16;
    u.iterate_origin16 = IterateOrigin16;
    return u;
  }();
  return &utils;
}

}  // namespace fsp_shim
