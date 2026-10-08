#ifndef SNAPPER_EDIT_VIDEO_FILE_FRAMES_H_
#define SNAPPER_EDIT_VIDEO_FILE_FRAMES_H_

#include <QImage>
#include <QString>
#include <QtClassHelperMacros>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/frame.h"
#include "media/video_reader.h"
#include "render/video_frames.h"

namespace snapper {

// More open files than any reel needs at one frame; past it the oldest
// is closed.
constexpr int kMaxOpenVideos = 8;

// Hands the renderer pictures read straight from video files, keeping
// each file open so playing forward decodes every picture once. A file
// that can't be read is remembered and draws nothing. One per thread:
// the preview has one, each export makes its own.
class VideoFileFrames final : public VideoFrames {
 public:
  VideoFileFrames() = default;
  ~VideoFileFrames() override = default;
  Q_DISABLE_COPY_MOVE(VideoFileFrames)

  QImage Picture(const QString& path, Frame frame) override;
  // Paths that failed to open since the last Forget.
  const std::set<QString>& broken() const { return broken_; }
  // Closes every file, so moved or re-rendered files are read afresh.
  void Forget();

 private:
  VideoReader* ReaderFor(const QString& path);

  std::map<QString, std::unique_ptr<VideoReader>> readers_;
  // When each open file was last read, for closing the oldest.
  std::map<QString, int> last_use_;
  std::set<QString> broken_;
  int uses_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_VIDEO_FILE_FRAMES_H_
