#include "render/warp_cache.h"

#include <cassert>

namespace snapper {

const WarpedImage& WarpCache::Bent(const QString& path, const QImage& source,
                                   WarpGrid grid,
                                   const std::vector<QPointF>& points) {
  assert(!source.isNull());
  assert(bytes_ >= 0);
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const bool is_match =
        it->grid == grid && it->path == path && it->points == points;
    if (is_match) {
      entries_.splice(entries_.begin(), entries_, it);
      return entries_.front().warped;
    }
  }
  entries_.push_front({path, grid, points, WarpImage(source, grid, points)});
  bytes_ += entries_.front().warped.image.sizeInBytes();
  Trim();
  return entries_.front().warped;
}

void WarpCache::Forget(const QString& path) {
  assert(bytes_ >= 0);
  assert(entries_.size() <= kMaxWarpEntries);
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
