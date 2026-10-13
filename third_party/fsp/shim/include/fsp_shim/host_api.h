// The calls Snapper's After Effects stand-in makes available to plugin
// code directly, behind the PF_ macros (fills, copies, handles) and the
// loud failure every unsupported call ends in.

#ifndef FSP_SHIM_HOST_API_H_
#define FSP_SHIM_HOST_API_H_

#include <cstddef>

#include "fsp_shim/ae_callbacks.h"

// Prints "fsp: <what>" to stderr, keeps it as the render's error message
// and gives back PF_Err_BAD_CALLBACK_PARAM. Every call the stand-in does
// not carry ends here, so a gap never passes silently.
PF_Err fsp_shim_fail(const char* what);
// The same for a call Snapper does not carry yet, by name.
PF_Err fsp_shim_unsupported(const char* call);

PF_Handle fsp_shim_new_handle(A_u_long size);
void* fsp_shim_lock_handle(PF_Handle handle);
void fsp_shim_unlock_handle(PF_Handle handle);
void fsp_shim_dispose_handle(PF_Handle handle);
A_u_long fsp_shim_handle_size(PF_Handle handle);
PF_Err fsp_shim_resize_handle(A_u_long size, PF_Handle* handle);

// A null color clears to transparent black, a null rect means all of it.
PF_Err fsp_shim_fill8(const PF_Pixel* color, const PF_Rect* rect,
                      PF_EffectWorld* world);
// Copies src_rect of src onto dst_rect of dst (null rects: all of the
// world), stretching with nearest pixels when the sizes differ.
PF_Err fsp_shim_copy(const PF_EffectWorld* src, PF_EffectWorld* dst,
                     const PF_Rect* src_rect, const PF_Rect* dst_rect);

// Copies src into dst of size bytes, cutting it short and always ending
// it, as PF_STRNNCPY does.
void fsp_shim_strnncpy(A_char* dst, const A_char* src, std::size_t size);

#endif  // FSP_SHIM_HOST_API_H_
