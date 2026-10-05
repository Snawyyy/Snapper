#ifndef SNAPPER_RENDER_FRAME_RENDERER_H_
#define SNAPPER_RENDER_FRAME_RENDERER_H_

#include <QImage>
#include <QTransform>

#include "base/frame.h"
#include "model/project.h"
#include "render/image_cache.h"

namespace snapper {

// Draws frames. The preview and the exporter both use this, so what is
// on screen is what gets exported. Not shared between threads: each
// thread makes its own (with its own image cache).
class FrameRenderer final {
 public:
  // The master track at frame: shots, their transitions and effects.
  // scale is the output size against the canvas: 1 for export, less
  // for a faster preview. Past the last shot the frame is black.
  QImage RenderFrame(const Project& project, Frame master, double scale);
  // One shot on its own, at a frame of its own.
  QImage RenderShot(const Project& project, const Shot& shot, Frame local,
                    double scale);

  // Shot space (origin at the canvas middle) to output pixels, with the
  // shot's camera at local. Tools use it to hit-test what was drawn.
  static QTransform ViewTransform(const Project& project, const Shot& shot,
                                  Frame local, double scale);

  ImageCache* cache() { return &cache_; }

 private:
  ImageCache cache_;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_FRAME_RENDERER_H_
