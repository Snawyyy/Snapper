#ifndef SNAPPER_UI_SHOT_THUMBS_H_
#define SNAPPER_UI_SHOT_THUMBS_H_

#include <QPixmap>

#include <map>
#include <memory>

#include "model/project.h"
#include "render/frame_renderer.h"

namespace snapper {

// Pictures of each shot's first frame, drawn once for the shot as it is.
// An edited shot gets a new picture, but not while an edit is still
// running (a drag), so posing never waits on them: the old picture stays
// until the step lands.
class ShotThumbs final {
 public:
  // The picture of shot, as wide as its longer side is long to width.
  // is_editing keeps a picture drawn before for the shot, whatever has
  // changed since.
  QPixmap Of(const Project& project,
             const std::shared_ptr<const Shot>& shot, int width,
             bool is_editing);

 private:
  struct Entry final {
    std::shared_ptr<const Shot> from;
    int width = 0;
    QPixmap picture;
  };

  FrameRenderer renderer_;
  std::map<int, Entry> pictures_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_THUMBS_H_
