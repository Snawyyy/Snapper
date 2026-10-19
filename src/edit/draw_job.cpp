// DrawManager: draw mode on a picture another manager hands in (see
// DrawJob in draw_manager.h).

#include <cassert>

#include "base/text.h"
#include "edit/draw_manager.h"
#include "edit/playback_manager.h"

namespace snapper {

Result<void> DrawManager::BeginJob(DrawJob job) {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  if (is_drawing_) {
    return std::unexpected(
        Error{Tr("Already drawing: press Done or Cancel first.")});
  }
  const bool has_picture = !job.image.isNull();
  const bool has_keeper = job.keep != nullptr;
  const bool is_usable = has_picture && has_keeper;
  if (!is_usable) {
    return std::unexpected(Error{Tr("There is nothing to draw on.")});
  }
  const bool was_playing = playback_->IsPlaying();
  if (was_playing) {
    playback_->Pause();
  }
  image_ = job.image.convertToFormat(QImage::Format_ARGB32);
  reference_ = job.reference.isNull() ? image_ : job.reference;
  under_ = job.under;
  onions_ = std::move(job.onions);
  keep_ = std::move(job.keep);
  // The frame before shows faint from the start, to trace from (D turns
  // it off).
  const bool has_onions = !onions_.empty();
  if (has_onions) {
    is_onion_ = true;
  }
  title_ = job.title;
  Open();
  return {};
}

}  // namespace snapper
