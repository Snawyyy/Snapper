#ifndef SNAPPER_RENDER_WARP_CACHE_H_
#define SNAPPER_RENDER_WARP_CACHE_H_

#include <QImage>
#include <QPointF>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <utility>
#include <vector>

#include "model/doll.h"
#include "render/warp_raster.h"

namespace snapper {

// Past this many bytes of bent drawings the least recently used goes.
constexpr std::int64_t kWarpCacheBytes = 256LL * 1024 * 1024;
// A pose rarely has more warped pieces than this; the cap keeps the
// lookup short.
constexpr size_t kMaxWarpEntries = 64;
// A bend at this many quarters is the whole drawing.
constexpr int kFullDetail = 4;
// A smooth bend may be made up to this many quarters (twice the
// drawing's size), so zooming in shows smooth curves; past it the
// painter blends the rest.
constexpr int kSmoothDetail = 8;

// Bent drawings kept by what bent them. Bending a drawing is the
// slowest part of a frame, and a frame that only moves the cursor, or
// moves a piece that has no warp, bends the same drawings again.
class WarpCache final {
 public:
  // path's source bent by grid over points, at detail quarters of its
  // size (1 to 4, 4 is whole; a blended look up to kSmoothDetail). A
  // smaller bend is faster and is meant to be drawn stretched back (see
  // WarpedImage::detail). A blended look bends an upscaled copy of
  // source (see Upscaled). The reference holds until the next call.
  const WarpedImage& Bent(const QString& path, const QImage& source,
                          const WarpGrid& grid,
                          const std::vector<QPointF>& points,
                          int detail = kFullDetail,
                          const WarpLook& look = {});
  // source upscaled times (2 or 4) by UpscaleDrawing, kept (the least
  // recently used go first past kWarpCacheBytes). The reference holds
  // until the next call.
  const QImage& Upscaled(const QString& path, const QImage& source,
                         int times);
  // Forgets every bend of path, for when its file changed.
  void Forget(const QString& path);
  size_t size() const { return entries_.size(); }
  std::int64_t bytes() const { return bytes_; }

 private:
  struct Entry final {
    QString path;
    WarpGrid grid;
    int detail;
    WarpLook look;
    std::vector<QPointF> points;
    WarpedImage warped;
  };
  struct Upscale final {
    QString path;
    int times;
    QImage image;
  };

  void Trim();
  // source made detail quarters of its size, kept for the next bend.
  const QImage& Shrunk(const QString& path, const QImage& source,
                       int detail);

  // Most recently used first.
  std::list<Entry> entries_;
  std::int64_t bytes_ = 0;
  std::map<std::pair<QString, int>, QImage> shrunk_;
  // Most recently used first.
  std::list<Upscale> upscaled_;
  std::int64_t upscaled_bytes_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_WARP_CACHE_H_
