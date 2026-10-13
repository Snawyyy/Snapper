#ifndef SNAPPER_RENDER_FRAME_RENDERER_H_
#define SNAPPER_RENDER_FRAME_RENDERER_H_

#include <QColor>
#include <QImage>
#include <QTransform>

#include <map>
#include <set>

#include "base/frame.h"
#include "model/project.h"
#include "render/comp.h"
#include "render/image_cache.h"
#include "render/video_frames.h"

namespace snapper {

// Draws frames. The preview and the exporter both use this, so what is
// on screen is what gets exported. Not shared between threads: each
// thread makes its own (with its own image cache).
class FrameRenderer final {
 public:
  // The master track at frame: the shots one after another, with their
  // effects.
  // scale is the output size against the canvas: 1 for export, less
  // for a faster preview. Past the last shot the frame is black.
  // under, when given, is drawn over the background and below every
  // layer: the Pose tab stage passes its references there. Nothing that
  // makes the shot's own pictures (export, reel, thumbnails) passes it.
  QImage RenderFrame(const Project& project, Frame master, double scale,
                     const QImage* under = nullptr);
  // The reel (the final video) at frame: each track's clip, bottom
  // first, a shot clip drawn as its shot alone, a video fitted to the
  // canvas and a text clip's words over what is below (PaintReelText).
  // Where nothing plays the frame is black. Over a transition
  // the clip and the next one are mixed. videos supplies the video
  // pictures; with nullptr, videos are left out. under is what lies
  // below every track: black, or clear for an export that keeps alpha.
  // A see-through shot clip shows the tracks below it.
  QImage RenderReel(const Project& project, Frame frame, double scale,
                    VideoFrames* videos, QColor under = Qt::black);
  // One shot on its own, at a frame of its own. A see-through shot
  // (Shot::is_transparent) leaves its empty area clear. under as in
  // RenderFrame.
  QImage RenderShot(const Project& project, const Shot& shot, Frame local,
                    double scale, const QImage* under = nullptr);

  // Only some layers of a shot on a clear picture, with no effects: a
  // layer that maps to an empty set is drawn whole, one that maps to
  // piece names only draws those pieces of its doll.
  QImage RenderLayers(const Project& project, const Shot& shot, Frame local,
                      double scale,
                      const std::map<LayerId, std::set<QString>>& only);

  // Every layer of a shot but effects on a clear picture covering area
  // (output pixels, the canvas corner at 0,0), which may reach past the
  // canvas on any side: the stage's pasteboard. The camera is used
  // without its shake, so the outside holds still while posing.
  QImage RenderArea(const Project& project, const Shot& shot, Frame local,
                    double scale, const QRect& area);

  // Shot space (origin at the canvas middle) to output pixels, with the
  // shot's camera at local. Tools use it to hit-test what was drawn.
  static QTransform ViewTransform(const Project& project, const Shot& shot,
                                  Frame local, double scale);

  ImageCache* cache() { return &cache_; }

  // How the Comp tab's stage shows the comp (V and K); export and every
  // other renderer keep the default, the whole comp.
  void SetCompShow(const CompShow& show) { comp_show_ = show; }
  // What each comp effect cost in the last frame, in milliseconds, and
  // which plugin effects refused to run.
  const CompReport& comp_report() const { return comp_report_; }

 private:
  // Draws layer onto frame through its own picture, keeping the comp's
  // target pictures (and the drawn shape) as needs asks.
  void PaintForComp(const Project& project, const Layer& layer, Frame local,
                    double scale, const QTransform& view, LinkSolver* links,
                    const CompNeeds& needs, CompPictures* pictures,
                    QImage* frame);

  // Draws clip's source at source over out: a shot whole, a video
  // fitted to the canvas, text where its look places it.
  void DrawClip(const Project& project, const Clip& clip, Frame source,
                double scale, VideoFrames* videos, QImage* out);

  ImageCache cache_;
  CompShow comp_show_;
  CompReport comp_report_;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_FRAME_RENDERER_H_
