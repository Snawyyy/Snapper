#ifndef SNAPPER_RENDER_IMAGE_CACHE_H_
#define SNAPPER_RENDER_IMAGE_CACHE_H_

#include <QImage>
#include <QString>

#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

#include "anim/joint_snap.h"
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
  // Hit tests use the default look, so they read the original drawing.
  const WarpedImage& Warped(const QString& path, const WarpGrid& grid,
                            const std::vector<QPointF>& points,
                            int detail = kFullDetail,
                            const WarpLook& look = {});
  // The drawing at path upscaled times (2 or 4) for the Smooth look
  // (see UpscaleDrawing); null if path can't be read. The reference
  // holds until the next call.
  const QImage& Upscaled(const QString& path, int times);
  // Forgets path, for when a file changed on disk.
  void Forget(const QString& path);
  // While on, every drawing paints Crisp whatever its doll's look: the
  // stage's hold-to-compare key.
  void set_crisp_only(bool is_on) { is_crisp_only_ = is_on; }
  bool is_crisp_only() const { return is_crisp_only_; }
  std::int64_t bytes() const { return bytes_; }
  // The poses solved so far for the shot being drawn (see PoseMemo).
  PoseMemo* poses() { return &poses_; }
  // The drawings measured for joint circles (see LimbMemo).
  LimbMemo* limbs() { return &limbs_; }

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
  LimbMemo limbs_;
  bool is_crisp_only_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_IMAGE_CACHE_H_
