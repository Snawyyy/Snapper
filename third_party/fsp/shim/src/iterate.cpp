// The iterate calls. Work is spread over a few threads, as After Effects
// does: plugin code is written for its callbacks to run at once. A
// PF_Err thrown by a callback stops the run and is returned.

#include "fsp_shim/iterate.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "fsp_shim/host_api.h"
#include "fsp_shim/pixels.h"
#include "fsp_shim/session.h"

namespace {

constexpr A_long kMaxThreads = 16;

A_long ThreadCount(A_long work) {
  const A_long cores =
      static_cast<A_long>(std::thread::hardware_concurrency());
  const A_long wanted = std::clamp<A_long>(cores, 1, kMaxThreads);
  return std::clamp<A_long>(work, 1, wanted);
}

PF_Err CallSafely(const auto& job, A_long thread, A_long index) {
  try {
    return job(thread, index);
  } catch (PF_Err thrown) {
    return thrown;
  } catch (...) {
    return fsp_shim_fail("a plugin callback threw an unknown exception");
  }
}

// Runs job(thread, index) for every index in [0, count) on up to
// kMaxThreads threads; gives back the first error.
PF_Err ParallelFor(A_long count, const auto& job) {
  std::atomic<A_long> next{0};
  std::atomic<PF_Err> first_err{PF_Err_NONE};
  auto worker = [&](A_long thread) {
    for (A_long i = next++; i < count; i = next++) {
      const bool has_failed = first_err.load() != PF_Err_NONE;
      if (has_failed) {
        return;
      }
      const PF_Err err = CallSafely(job, thread, i);
      PF_Err none = PF_Err_NONE;
      const bool is_err = err != PF_Err_NONE;
      if (is_err) {
        first_err.compare_exchange_strong(none, err);
      }
    }
  };
  const A_long threads = ThreadCount(count);
  std::vector<std::jthread> pool;
  pool.reserve(static_cast<std::size_t>(threads));
  for (A_long t = 1; t < threads; ++t) {
    pool.emplace_back(worker, t);
  }
  worker(0);
  pool.clear();
  return first_err.load();
}

// The area of dst to visit: rect clipped to dst (null: all of it).
PF_Rect VisitRect(const PF_EffectWorld* dst, const PF_Rect* area) {
  PF_Rect rect{0, 0, dst->width, dst->height};
  const bool has_area = area != nullptr;
  if (has_area) {
    rect.left = std::max(rect.left, area->left);
    rect.top = std::max(rect.top, area->top);
    rect.right = std::min(rect.right, area->right);
    rect.bottom = std::min(rect.bottom, area->bottom);
  }
  return rect;
}

template <typename Pixel, typename Func>
PF_Err IteratePixels(PF_EffectWorld* src, const PF_Rect* area,
                     const PF_Point* origin, void* refcon, Func pix_fn,
                     PF_EffectWorld* dst) {
  const bool is_bad = dst == nullptr || pix_fn == nullptr;
  if (is_bad) {
    return fsp_shim_fail("iterate: null output world or function");
  }
  const PF_Rect rect = VisitRect(dst, area);
  const bool is_empty = rect.right <= rect.left ||
                        rect.bottom <= rect.top || dst->data == nullptr;
  if (is_empty) {
    return PF_Err_NONE;
  }
  const A_long dx = origin != nullptr ? origin->h : 0;
  const A_long dy = origin != nullptr ? origin->v : 0;
  auto row = [&](A_long thread, A_long index) -> PF_Err {
    (void)thread;
    const A_long y = rect.top + index;
    for (A_long x = rect.left; x < rect.right; ++x) {
      Pixel* out = fsp_shim::PixelAt<Pixel>(dst, x, y);
      Pixel outside{};
      Pixel* in = out;
      const bool has_src = src != nullptr && src->data != nullptr;
      if (has_src) {
        const A_long sx = x + dx;
        const A_long sy = y + dy;
        const bool is_inside =
            sx >= 0 && sy >= 0 && sx < src->width && sy < src->height;
        in = is_inside ? fsp_shim::PixelAt<Pixel>(src, sx, sy) : &outside;
      }
      const PF_Err err = pix_fn(refcon, x, y, in, out);
      const bool is_err = err != PF_Err_NONE;
      if (is_err) {
        return err;
      }
    }
    return PF_Err_NONE;
  };
  return ParallelFor(rect.bottom - rect.top, row);
}

}  // namespace

namespace fsp_shim {

PF_Err Iterate8(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                const PF_Rect* area, void* refcon,
                PF_IteratePixel8Func pix_fn, PF_EffectWorld* dst) {
  return IteratePixels<PF_Pixel>(src, area, nullptr, refcon, pix_fn, dst);
}

PF_Err IterateOrigin8(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                      const PF_Rect* area, const PF_Point* origin,
                      void* refcon, PF_IteratePixel8Func pix_fn,
                      PF_EffectWorld* dst) {
  return IteratePixels<PF_Pixel>(src, area, origin, refcon, pix_fn, dst);
}

PF_Err Iterate16(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                 const PF_Rect* area, void* refcon,
                 PF_IteratePixel16Func pix_fn, PF_EffectWorld* dst) {
  return IteratePixels<PF_Pixel16>(src, area, nullptr, refcon, pix_fn, dst);
}

PF_Err IterateOrigin16(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                       const PF_Rect* area, const PF_Point* origin,
                       void* refcon, PF_IteratePixel16Func pix_fn,
                       PF_EffectWorld* dst) {
  return IteratePixels<PF_Pixel16>(src, area, origin, refcon, pix_fn, dst);
}

PF_Err IterateFloat(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                    const PF_Rect* area, void* refcon,
                    PF_IteratePixelFloatFunc pix_fn, PF_EffectWorld* dst) {
  return IteratePixels<PF_PixelFloat>(src, area, nullptr, refcon, pix_fn,
                                      dst);
}

PF_Err IterateOriginFloat(PF_InData*, A_long, A_long, PF_EffectWorld* src,
                          const PF_Rect* area, const PF_Point* origin,
                          void* refcon, PF_IteratePixelFloatFunc pix_fn,
                          PF_EffectWorld* dst) {
  return IteratePixels<PF_PixelFloat>(src, area, origin, refcon, pix_fn,
                                      dst);
}

PF_Err IterateGeneric(A_long iterations, void* refcon,
                      PF_IterateGenericFunc fn_func) {
  const bool is_bad = fn_func == nullptr || iterations < -1;
  if (is_bad) {
    return fsp_shim_fail("iterate_generic: null function or bad count");
  }
  const bool is_per_thread =
      iterations == PF_Iterations_ONCE_PER_PROCESSOR;
  if (is_per_thread) {
    const A_long threads = ThreadCount(kMaxThreads);
    return ParallelFor(threads, [&](A_long, A_long index) {
      return fn_func(refcon, index, index, iterations);
    });
  }
  return ParallelFor(iterations, [&](A_long thread, A_long index) {
    return fn_func(refcon, thread, index, iterations);
  });
}

}  // namespace fsp_shim
