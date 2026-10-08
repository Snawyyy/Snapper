#ifndef SNAPPER_UI_REEL_PREVIEW_H_
#define SNAPPER_UI_REEL_PREVIEW_H_

#include <QWidget>

#include "edit/video_file_frames.h"
#include "render/frame_renderer.h"
#include "ui/managers.h"

namespace snapper {

// The final video at the reel's playhead, as big as fits. It only shows:
// shots are posed on the Pose tab, clips are cut on the timeline below.
class ReelPreview final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelPreview(const Managers& managers);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Managers managers_;
  FrameRenderer renderer_;
  VideoFileFrames videos_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_PREVIEW_H_
