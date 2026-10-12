#include "render/warp_cache.h"

#include <algorithm>
#include <cassert>
#include <iterator>

#include "render/upscale.h"

namespace snapper {

const QImage& WarpCache::Shrunk(const QString& path, const QImage& source,
                                int detail) {
  assert(detail >= 1 && detail < kFullDetail);
  assert(!source.isNull());
  const auto key = std::make_pair(path, detail);
  const auto found = shrunk_.find(key);
  const bool is_kept = found != shrunk_.end();
  if (is_kept) {
    return found->second;
  }
  const QSize size(std::max(1, source.width() * detail / kFullDetail),
                   std::max(1, source.height() * detail / kFullDetail));
  return shrunk_
      .emplace(key, source.scaled(size, Qt::IgnoreAspectRatio,
                                  Qt::FastTransformation))
      .first->second;
}

const QImage& WarpCache::Upscaled(const QString& path, const QImage& source,
                                  int times) {
  assert(times == 2 || times == kMaxUpscale);
  assert(!source.isNull());
  for (auto it = upscaled_.begin(); it != upscaled_.end(); ++it) {
    const bool is_match = it->times == times && it->path == path;
    if (is_match) {
      upscaled_.splice(upscaled_.begin(), upscaled_, it);
      return upscaled_.front().image;
    }
  }
  upscaled_.push_front({path, times, UpscaleDrawing(source, times)});
  upscaled_bytes_ += upscaled_.front().image.sizeInBytes();
  // Never drops the one just made, even if it alone is too big.
  while (upscaled_.size() > 1 && upscaled_bytes_ > kWarpCacheBytes) {
    upscaled_bytes_ -= upscaled_.back().image.sizeInBytes();
    upscaled_.pop_back();
  }
  return upscaled_.front().image;
}

const WarpedImage& WarpCache::Bent(const QString& path, const QImage& source,
                                   const WarpGrid& grid,
                                   const std::vector<QPointF>& points,
                                   int detail, const WarpLook& look) {
  assert(!source.isNull());
  assert(detail >= 1 &&
         detail <= (look.is_blended ? kSmoothDetail : kFullDetail));
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const bool is_match = it->grid == grid && it->detail == detail &&
                          it->look == look && it->path == path &&
                          it->points == points;
    if (is_match) {
      entries_.splice(entries_.begin(), entries_, it);
      return entries_.front().warped;
    }
  }
  const bool is_whole = detail == kFullDetail;
  WarpedImage bent;
  if (look.is_blended) {
    // Bent from the upscaled copy straight to detail's size: the rest
    // corners are measured on the copy, the points scaled to the bend.
    const double factor = static_cast<double>(detail) / kFullDetail;
    std::vector<QPointF> scaled = points;
    for (QPointF& point : scaled) {
      point *= factor;
    }
    const QImage& big =
        Upscaled(path, source, UpscaleTimes(source.size(), factor));
    bent = WarpImage(big, grid, scaled, look);
    bent.origin /= factor;
    bent.detail = factor;
  } else if (is_whole) {
    bent = WarpImage(source, grid, points);
  } else {
    const double factor = static_cast<double>(detail) / kFullDetail;
    std::vector<QPointF> small = points;
    for (QPointF& point : small) {
      point *= factor;
    }
    bent = WarpImage(Shrunk(path, source, detail), grid, small);
    bent.origin /= factor;
    bent.detail = factor;
  }
  entries_.push_front({path, grid, detail, look, points, std::move(bent)});
  bytes_ += entries_.front().warped.image.sizeInBytes();
  Trim();
  return entries_.front().warped;
}

void WarpCache::Forget(const QString& path) {
  assert(bytes_ >= 0);
  assert(entries_.size() <= kMaxWarpEntries);
  for (auto it = shrunk_.begin(); it != shrunk_.end();) {
    const bool is_path = it->first.first == path;
    it = is_path ? shrunk_.erase(it) : std::next(it);
  }
  for (auto it = upscaled_.begin(); it != upscaled_.end();) {
    const bool is_path = it->path == path;
    if (is_path) {
      upscaled_bytes_ -= it->image.sizeInBytes();
      it = upscaled_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = entries_.begin(); it != entries_.end();) {
    const bool is_path = it->path == path;
    if (is_path) {
      bytes_ -= it->warped.image.sizeInBytes();
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

void WarpCache::Trim() {
  assert(!entries_.empty());
  assert(bytes_ >= 0);
  // Never drops the bend just asked for, even if it alone is too big.
  while (entries_.size() > 1) {
    const bool is_over =
        bytes_ > kWarpCacheBytes || entries_.size() > kMaxWarpEntries;
    if (!is_over) {
      return;
    }
    bytes_ -= entries_.back().warped.image.sizeInBytes();
    entries_.pop_back();
  }
}

}  // namespace snapper
