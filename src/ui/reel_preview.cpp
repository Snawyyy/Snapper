#include "ui/reel_preview.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/theme.h"

namespace snapper {

ReelPreview::ReelPreview(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  setMinimumSize(160, 90);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  assert(minimumWidth() > 0);
}

void ReelPreview::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  const bool is_empty = IsReelEmpty(project.reel);
  if (is_empty) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Drag shots from the list, or a video file, onto "
                        "the tracks below."));
    return;
  }
  const double scale =
      std::min(static_cast<double>(width()) / project.canvas.width,
               static_cast<double>(height()) / project.canvas.height);
  const QImage frame = renderer_.RenderReel(
      project, managers_.playback->FrameOn(Timeline::kReel),
      std::max(scale, 0.01), &videos_);
  painter.drawImage((width() - frame.width()) / 2,
                    (height() - frame.height()) / 2, frame);
}

}  // namespace snapper
