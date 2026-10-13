// Pictures (worlds): making and freeing them, and the whole-picture
// operations After Effects does for a plugin (fill, copy, blend,
// premultiply) in all three pixel depths.

#include <cassert>
#include <cstring>
#include <memory>

#include "fsp_shim/host_api.h"
#include "fsp_shim/pixels.h"
#include "fsp_shim/session.h"

namespace fsp_shim {

A_long PixelBytes(PF_PixelFormat format) {
  switch (format) {
    case PF_PixelFormat_ARGB32:
      return static_cast<A_long>(sizeof(PF_Pixel));
    case PF_PixelFormat_ARGB64:
      return static_cast<A_long>(sizeof(PF_Pixel16));
    case PF_PixelFormat_ARGB128:
      return static_cast<A_long>(sizeof(PF_PixelFloat));
    default:
      return 0;
  }
}

PF_PixelFormat WorldFormat(const PF_EffectWorld* world) {
  assert(world != nullptr);
  const bool is_float = (world->world_flags & PF_WorldFlag_FSP_FLOAT) != 0;
  if (is_float) {
    return PF_PixelFormat_ARGB128;
  }
  const bool is_deep = (world->world_flags & PF_WorldFlag_DEEP) != 0;
  return is_deep ? PF_PixelFormat_ARGB64 : PF_PixelFormat_ARGB32;
}

PF_Err NewWorld(FspShimEffectRef* ref, A_long width, A_long height,
                PF_PixelFormat format, PF_EffectWorld* world) {
  const A_long bytes = PixelBytes(format);
  const bool is_bad = ref == nullptr || world == nullptr || width < 0 ||
                      height < 0 || bytes == 0;
  if (is_bad) {
    return fsp_shim_fail("new world: bad size, format or effect_ref");
  }
  std::memset(world, 0, sizeof(*world));
  world->width = width;
  world->height = height;
  world->rowbytes = width * bytes;
  world->extent_hint = PF_Rect{0, 0, width, height};
  world->pix_aspect_ratio = PF_RationalScale{1, 1};
  world->world_flags = PF_WorldFlag_WRITEABLE;
  const bool is_deep = format == PF_PixelFormat_ARGB64;
  if (is_deep) {
    world->world_flags |= PF_WorldFlag_DEEP;
  }
  const bool is_float = format == PF_PixelFormat_ARGB128;
  if (is_float) {
    world->world_flags |= PF_WorldFlag_FSP_FLOAT;
  }
  const std::size_t size =
      static_cast<std::size_t>(world->rowbytes) *
      static_cast<std::size_t>(height);
  const bool is_empty = size == 0;
  if (is_empty) {
    return PF_Err_NONE;
  }
  auto pixels = std::make_unique<unsigned char[]>(size);
  world->data = reinterpret_cast<PF_PixelPtr>(pixels.get());
  const std::lock_guard<std::mutex> lock(ref->worlds_mutex);
  ref->worlds[pixels.get()] = std::move(pixels);
  return PF_Err_NONE;
}

PF_Err DisposeWorld(FspShimEffectRef* ref, PF_EffectWorld* world) {
  const bool is_bad = ref == nullptr || world == nullptr;
  if (is_bad) {
    return fsp_shim_fail("dispose world: null world or effect_ref");
  }
  const bool has_pixels = world->data != nullptr;
  if (has_pixels) {
    const std::lock_guard<std::mutex> lock(ref->worlds_mutex);
    const bool is_ours = ref->worlds.erase(world->data) == 1;
    if (!is_ours) {
      return fsp_shim_fail("dispose world: not a world this host made");
    }
  }
  std::memset(world, 0, sizeof(*world));
  return PF_Err_NONE;
}

}  // namespace fsp_shim

namespace {

using fsp_shim::PixelAt;
using fsp_shim::WorldFormat;

// The part of world inside rect (null: all of it).
PF_Rect ClipRect(const PF_EffectWorld* world, const PF_Rect* rect) {
  PF_Rect all{0, 0, world->width, world->height};
  const bool is_all = rect == nullptr;
  if (is_all) {
    return all;
  }
  PF_Rect clipped = *rect;
  clipped.left = clipped.left < 0 ? 0 : clipped.left;
  clipped.top = clipped.top < 0 ? 0 : clipped.top;
  clipped.right = clipped.right > all.right ? all.right : clipped.right;
  clipped.bottom = clipped.bottom > all.bottom ? all.bottom : clipped.bottom;
  return clipped;
}

template <typename Pixel>
void FillWith(const Pixel& color, const PF_Rect& rect,
              PF_EffectWorld* world) {
  for (A_long y = rect.top; y < rect.bottom; ++y) {
    for (A_long x = rect.left; x < rect.right; ++x) {
      *PixelAt<Pixel>(world, x, y) = color;
    }
  }
}

template <typename Pixel>
void CopyRect(const PF_EffectWorld* src, PF_EffectWorld* dst,
              const PF_Rect& from, const PF_Rect& to) {
  const A_long from_w = from.right - from.left;
  const A_long from_h = from.bottom - from.top;
  const A_long to_w = to.right - to.left;
  const A_long to_h = to.bottom - to.top;
  for (A_long y = 0; y < to_h; ++y) {
    const A_long sy = from.top + y * from_h / to_h;
    for (A_long x = 0; x < to_w; ++x) {
      const A_long sx = from.left + x * from_w / to_w;
      *PixelAt<Pixel>(dst, to.left + x, to.top + y) =
          *PixelAt<Pixel>(src, sx, sy);
    }
  }
}

}  // namespace

PF_Err fsp_shim_fill8(const PF_Pixel* color, const PF_Rect* rect,
                      PF_EffectWorld* world) {
  const bool is_bad = world == nullptr;
  if (is_bad) {
    return fsp_shim_fail("fill: null world");
  }
  const bool has_pixels = world->data != nullptr;
  if (!has_pixels) {
    return PF_Err_NONE;
  }
  const PF_Pixel clear{0, 0, 0, 0};
  const PF_Pixel paint = color != nullptr ? *color : clear;
  const PF_Rect area = ClipRect(world, rect);
  switch (WorldFormat(world)) {
    case PF_PixelFormat_ARGB64:
      FillWith(fsp_shim::To16(paint), area, world);
      break;
    case PF_PixelFormat_ARGB128:
      FillWith(fsp_shim::ToFloat(paint), area, world);
      break;
    default:
      FillWith(paint, area, world);
      break;
  }
  return PF_Err_NONE;
}

PF_Err fsp_shim_copy(const PF_EffectWorld* src, PF_EffectWorld* dst,
                     const PF_Rect* src_rect, const PF_Rect* dst_rect) {
  const bool is_bad = src == nullptr || dst == nullptr;
  if (is_bad) {
    return fsp_shim_fail("copy: null world");
  }
  const PF_PixelFormat format = WorldFormat(src);
  const bool is_mixed = format != WorldFormat(dst);
  if (is_mixed) {
    return fsp_shim_unsupported("copy between pixel depths");
  }
  const PF_Rect from = ClipRect(src, src_rect);
  const PF_Rect to = ClipRect(dst, dst_rect);
  const bool is_empty = from.right <= from.left || from.bottom <= from.top ||
                        to.right <= to.left || to.bottom <= to.top ||
                        src->data == nullptr || dst->data == nullptr;
  if (is_empty) {
    return PF_Err_NONE;
  }
  switch (format) {
    case PF_PixelFormat_ARGB64:
      CopyRect<PF_Pixel16>(src, dst, from, to);
      break;
    case PF_PixelFormat_ARGB128:
      CopyRect<PF_PixelFloat>(src, dst, from, to);
      break;
    default:
      CopyRect<PF_Pixel>(src, dst, from, to);
      break;
  }
  return PF_Err_NONE;
}
