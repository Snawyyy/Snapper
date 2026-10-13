// The iterate calls: run a plugin's function over every pixel of a
// picture, or over a count of indexes, spread over threads as After
// Effects does.

#ifndef FSP_SHIM_ITERATE_H_
#define FSP_SHIM_ITERATE_H_

#include "fsp_shim/ae_callbacks.h"

namespace fsp_shim {

PF_Err Iterate8(PF_InData* in_data, A_long progress_base,
                A_long progress_final, PF_EffectWorld* src,
                const PF_Rect* area, void* refcon,
                PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst);
PF_Err IterateOrigin8(PF_InData* in_data, A_long progress_base,
                      A_long progress_final, PF_EffectWorld* src,
                      const PF_Rect* area, const PF_Point* origin,
                      void* refcon, PF_IteratePixel8Func pix_fn,
                      PF_EffectWorld* dst);
PF_Err Iterate16(PF_InData* in_data, A_long progress_base,
                 A_long progress_final, PF_EffectWorld* src,
                 const PF_Rect* area, void* refcon,
                 PF_IteratePixel16Func pix_fn, PF_EffectWorld* dst);
PF_Err IterateOrigin16(PF_InData* in_data, A_long progress_base,
                       A_long progress_final, PF_EffectWorld* src,
                       const PF_Rect* area, const PF_Point* origin,
                       void* refcon, PF_IteratePixel16Func pix_fn,
                       PF_EffectWorld* dst);
PF_Err IterateFloat(PF_InData* in_data, A_long progress_base,
                    A_long progress_final, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixelFloatFunc pix_fn, PF_EffectWorld* dst);
PF_Err IterateOriginFloat(PF_InData* in_data, A_long progress_base,
                          A_long progress_final, PF_EffectWorld* src,
                          const PF_Rect* area, const PF_Point* origin,
                          void* refcon, PF_IteratePixelFloatFunc pix_fn,
                          PF_EffectWorld* dst);
// fn_func(refcon, thread, i, iterations) for i in [0, iterations); with
// PF_Iterations_ONCE_PER_PROCESSOR, once per thread with i the thread.
PF_Err IterateGeneric(A_long iterations, void* refcon,
                      PF_IterateGenericFunc fn_func);

}  // namespace fsp_shim

#endif  // FSP_SHIM_ITERATE_H_
