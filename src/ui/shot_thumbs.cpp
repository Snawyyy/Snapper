#include "ui/shot_thumbs.h"

#include <algorithm>
#include <cassert>

namespace snapper {

QPixmap ShotThumbs::Of(const Project& project,
                       const std::shared_ptr<const Shot>& shot, int width,
                       bool is_editing) {
  assert(shot != nullptr);
  assert(width > 0);
  Entry& entry = pictures_[shot->id.value()];
  const bool has_picture = !entry.picture.isNull();
  const bool is_same = entry.from == shot && entry.width == width;
  const bool is_kept = has_picture && (is_same || is_editing);
  if (is_kept) {
    return entry.picture;
  }
  const double scale = static_cast<double>(width) /
                       std::max(project.canvas.width, project.canvas.height);
  entry.picture = QPixmap::fromImage(
      renderer_.RenderShot(project, *shot, Frame(0), scale));
  entry.from = shot;
  entry.width = width;
  return entry.picture;
}

}  // namespace snapper
