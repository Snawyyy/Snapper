#include "render/warp_cache.h"

#include <algorithm>
#include <cassert>
#include <iterator>

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
                                  Qt::SmoothTransformation))
      .first->second;
}

const WarpedImage& WarpCache::Bent(const QString& path, const QImage& source,
                                   WarpGrid grid,
                                   const std::vector<QPointF>& points,
                                   int detail) {
  assert(!source.isNull());
  assert(detail >= 1 && detail <= kFullDetail);
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const bool is_match = it->grid == grid && it->detail == detail &&
                          it->path == path && it->points == points;
    if (is_match) {
      entries_.splice(entries_.begin(), entries_, it);
      return entries_.front().warped;
    }
  }
  const bool is_whole = detail == kFullDetail;
  WarpedImage bent;
  if (is_whole) {
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
  entries_.push_front({path, grid, detail, points, std::move(bent)});
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
