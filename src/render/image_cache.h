#ifndef SNAPPER_RENDER_IMAGE_CACHE_H_
#define SNAPPER_RENDER_IMAGE_CACHE_H_

#include <QImage>
#include <QString>

#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "anim/pose_memo.h"
#include "model/doll.h"
#include "render/warp_cache.h"
#include "render/warp_raster.h"

namespace snapper {

// Past this many bytes of pixels the least recently drawn image goes.
constexpr std::int64_t kImageCacheBytes = 1024LL * 1024 * 1024;

// Drawings loaded from disk once and reused. Each renderer owns its
// own cache, so preview and an export thread never share one.
class ImageCache final {
 public:
  // The image at path, premultiplied for fast drawing; null if it
  // can't be read. A missing file is remembered so it isn't retried
  // every frame.
  const QImage& Get(const QString& path);
  // The drawing at path bent by grid over points (see WarpImage),
  // kept so the same bend is made once. An empty image if path can't
  // be read. The reference holds until the next call.
  const WarpedImage& Warped(const QString& path, const WarpGrid& grid,
                            const std::vector<QPointF>& points,
                            int detail = kFullDetail);
  // Forgets path, for when a file changed on disk.
  void Forget(const QString& path);
  std::int64_t bytes() const { return bytes_; }
  // The poses solved so far for the shot being drawn (see PoseMemo).
  PoseMemo* poses() { return &poses_; }

 private:
  struct Entry final {
    QImage image;
    std::list<QString>::iterator use;
  };

  void Trim();

  std::unordered_map<QString, Entry> images_;
  // Most recently used first.
  std::list<QString> uses_;
  std::int64_t bytes_ = 0;
  WarpCache warps_;
  PoseMemo poses_;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_IMAGE_CACHE_H_
