// Snapper's stand-in for the After Effects effect API: the helper macros
// plugin code is written against (error chaining, parameter adding,
// fixed point, handles, the C library). Plugin code expects macros here,
// so macros it is. Written for Snapper; see ae_types.h.

#ifndef FSP_SHIM_AE_MACROS_H_
#define FSP_SHIM_AE_MACROS_H_

#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "fsp_shim/ae_suite_helper.h"
#include "fsp_shim/host_api.h"

#define DllExport
#define AEFX_CLR_STRUCT(STRUCT) \
  do { std::memset(&(STRUCT), 0, sizeof(STRUCT)); } while (0)

// Run FUNC only while err is still clear.
#define ERR(FUNC) \
  do { if (!err) { err = (FUNC); } } while (0)
// Always run FUNC, keep the first error.
#define ERR2(FUNC) \
  do { if (((err2 = (FUNC)) != PF_Err_NONE) && !err) { err = err2; } } \
  while (0)

#ifndef ABS
#define ABS(N) ((N) < 0 ? -(N) : (N))
#endif
#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#define DH(HANDLE) (*(HANDLE))
#define PF_WORLD_IS_DEEP(WORLD) \
  (((WORLD)->world_flags & PF_WorldFlag_DEEP) != 0)

// ---------------------------------------------------------------------
// Fixed point (16.16).
#define INT2FIX(X) (static_cast<PF_Fixed>(X) * 65536)
#define FIX2INT(X) (static_cast<A_long>(X) >> 16)
#define FIX2INT_ROUND(X) (FIX2INT((X) + 32768))
#define FLOAT2FIX(F) \
  (static_cast<PF_Fixed>((F) * 65536.0 + (((F) < 0) ? -0.5 : 0.5)))
#define FIX_2_FLOAT(X) (static_cast<PF_FpLong>(X) / 65536.0)
#define FLOAT_2_FIX(F) FLOAT2FIX(F)
#define PF_Fixed_MAX 0x7fffffffL

#define PF_PI 3.14159265358979323846
#define PF_2PI 6.28318530717958647692
#define PF_HALF_PI 1.57079632679489661923
#define PF_E 2.7182818284590452354
#define PF_SQRT2 1.41421356237309504880
#define PF_RAD_PER_DEGREE (PF_PI / 180.0)

// ---------------------------------------------------------------------
// The C library. After Effects routes these through in_data->utils;
// the stand-in calls the library directly, which needs no in_data.
#define PF_SIN(X) std::sin(static_cast<double>(X))
#define PF_COS(X) std::cos(static_cast<double>(X))
#define PF_TAN(X) std::tan(static_cast<double>(X))
#define PF_ASIN(X) std::asin(static_cast<double>(X))
#define PF_ACOS(X) std::acos(static_cast<double>(X))
#define PF_ATAN(X) std::atan(static_cast<double>(X))
#define PF_ATAN2(Y, X) \
  std::atan2(static_cast<double>(Y), static_cast<double>(X))
#define PF_LOG(X) std::log(static_cast<double>(X))
#define PF_LOG10(X) std::log10(static_cast<double>(X))
#define PF_FLOOR(X) std::floor(static_cast<double>(X))
#define PF_CEIL(X) std::ceil(static_cast<double>(X))
#define PF_EXP(X) std::exp(static_cast<double>(X))
#define PF_FABS(X) std::fabs(static_cast<double>(X))
#define PF_HYPOT(X, Y) \
  std::hypot(static_cast<double>(X), static_cast<double>(Y))
#define PF_POW(X, Y) std::pow(static_cast<double>(X), static_cast<double>(Y))
#define PF_SQRT(X) std::sqrt(static_cast<double>(X))
#define PF_FMOD(X, Y) \
  std::fmod(static_cast<double>(X), static_cast<double>(Y))
#define PF_SPRINTF std::sprintf
#define PF_STRCPY(DST, SRC) std::strcpy((DST), (SRC))
#define PF_STRNNCPY(DST, SRC, SIZE) \
  fsp_shim_strnncpy((DST), (SRC), static_cast<std::size_t>(SIZE))

// ---------------------------------------------------------------------
// Handles and worlds, without needing in_data in scope.
#define PF_NEW_HANDLE(SIZE) fsp_shim_new_handle(static_cast<A_u_long>(SIZE))
#define PF_LOCK_HANDLE(HANDLE) fsp_shim_lock_handle(HANDLE)
#define PF_UNLOCK_HANDLE(HANDLE) fsp_shim_unlock_handle(HANDLE)
#define PF_DISPOSE_HANDLE(HANDLE) fsp_shim_dispose_handle(HANDLE)
#define PF_GET_HANDLE_SIZE(HANDLE) fsp_shim_handle_size(HANDLE)
#define PF_RESIZE_HANDLE(SIZE, HANDLEP) \
  fsp_shim_resize_handle(static_cast<A_u_long>(SIZE), (HANDLEP))
#define PF_FILL(COLOR, RECT, WORLD) fsp_shim_fill8((COLOR), (RECT), (WORLD))
#define PF_COPY(SRC, DST, SRC_RECT, DST_RECT) \
  fsp_shim_copy((SRC), (DST), (SRC_RECT), (DST_RECT))
#define PF_BLEND(SRC1, SRC2, RATIO, DST) \
  (*in_data->utils->blend)(in_data->effect_ref, (SRC1), (SRC2), (RATIO), \
                           (DST))
#define PF_ABORT(IN_DATA) (*(IN_DATA)->inter.abort)((IN_DATA)->effect_ref)
#define PF_PROGRESS(IN_DATA, CURRENT, TOTAL) \
  (*(IN_DATA)->inter.progress)((IN_DATA)->effect_ref, (CURRENT), (TOTAL))

#define PF_CHECKOUT_PARAM(IN_DATA, INDEX, TIME, STEP, SCALE, PARAM) \
  (*(IN_DATA)->inter.checkout_param)((IN_DATA)->effect_ref, (INDEX), \
                                     (TIME), (STEP), (SCALE), (PARAM))
#define PF_CHECKIN_PARAM(IN_DATA, PARAM) \
  (*(IN_DATA)->inter.checkin_param)((IN_DATA)->effect_ref, (PARAM))
#define PF_ADD_PARAM(IN_DATA, INDEX, DEF) \
  (*(IN_DATA)->inter.add_param)((IN_DATA)->effect_ref, (INDEX), (DEF))

#define PF_REGISTER_EFFECT_EXT2(DATA, CALLBACK, NAME, MATCH_NAME, CATEGORY, \
                                RESERVED, ENTRY, URL) \
  (*(CALLBACK))((DATA), reinterpret_cast<const A_u_char*>(NAME), \
                reinterpret_cast<const A_u_char*>(MATCH_NAME), \
                reinterpret_cast<const A_u_char*>(CATEGORY), \
                reinterpret_cast<const A_u_char*>(ENTRY), 0, 13, 28, \
                (RESERVED), reinterpret_cast<const A_u_char*>(URL))

// Smart_Utils: grow dst to hold src too.
inline void UnionLRect(const PF_LRect* src, PF_LRect* dst) {
  const bool dst_is_empty = dst->left >= dst->right ||
                            dst->top >= dst->bottom;
  if (dst_is_empty) {
    *dst = *src;
    return;
  }
  const bool src_is_empty = src->left >= src->right ||
                            src->top >= src->bottom;
  if (src_is_empty) {
    return;
  }
  dst->left = src->left < dst->left ? src->left : dst->left;
  dst->top = src->top < dst->top ? src->top : dst->top;
  dst->right = src->right > dst->right ? src->right : dst->right;
  dst->bottom = src->bottom > dst->bottom ? src->bottom : dst->bottom;
}

// Windows code calls min and max unqualified, of mixed types, as the
// Windows macros allow.
template <typename A, typename B>
constexpr std::common_type_t<A, B> min(A a, B b) {
  return b < a ? b : a;
}
template <typename A, typename B>
constexpr std::common_type_t<A, B> max(A a, B b) {
  return a < b ? b : a;
}

#endif  // FSP_SHIM_AE_MACROS_H_
