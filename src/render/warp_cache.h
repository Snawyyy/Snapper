#ifndef SNAPPER_RENDER_WARP_CACHE_H_
#define SNAPPER_RENDER_WARP_CACHE_H_

#include <QImage>
#include <QPointF>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

#include "model/doll.h"
#include "render/warp_raster.h"

namespace snapper {

// Past this many bytes of bent drawings the least recently used goes.
constexpr std::int64_t kWarpCacheBytes = 256LL * 1024 * 1024;
// A pose rarely has more warped pieces than this; the cap keeps the
// lookup short.
constexpr size_t kMaxWarpEntries = 64;

// Bent drawings kept by what bent them. Bending a drawing is the
// slowest part of a frame, and a frame that only moves the cursor, or
// moves a piece that has no warp, bends the same drawings again.
class WarpCache final {
 public:
  // path's source bent by grid over points. The reference holds until
  // the next call.
  const WarpedImage& Bent(const QString& path, const QImage& source,
                          WarpGrid grid, const std::vector<QPointF>& points);
  // Forgets every bend of path, for when its file changed.
  void Forget(const QString& path);
  size_t size() const { return entries_.size(); }
  std::int64_t bytes() const { return bytes_; }

 private:
  struct Entry final {
    QString path;
    WarpGrid grid;
    std::vector<QPointF> points;
    WarpedImage warped;
  };

  void Trim();

  // Most recently used first.
  std::list<Entry> entries_;
  std::int64_t bytes_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_WARP_CACHE_H_
