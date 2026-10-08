#ifndef SNAPPER_RENDER_VIDEO_FRAMES_H_
#define SNAPPER_RENDER_VIDEO_FRAMES_H_

#include <QImage>
#include <QString>

#include "base/frame.h"

namespace snapper {

// Where the renderer gets the pictures of video files. Drawing never
// opens files itself: the caller hands one of these in, so the same
// drawing code runs in the window, in an export thread and in a test.
// Not shared between threads.
class VideoFrames {
 public:
  virtual ~VideoFrames() = default;
  // The picture of the file at path at frame (from the file's start);
  // a null image when it can't be read.
  virtual QImage Picture(const QString& path, Frame frame) = 0;
};

}  // namespace snapper

#endif  // SNAPPER_RENDER_VIDEO_FRAMES_H_
