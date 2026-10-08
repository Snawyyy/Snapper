// ReelTimeline's drops and wheel.

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"

namespace snapper {
namespace {

constexpr double kWheelUnit = 120.0;
constexpr double kScrollPixels = 80.0;
// A video file's length is known only once it is read, so its drop
// outline shows two seconds.
constexpr int kFileDropFrames = 2 * kFramesPerSecond;

std::vector<QString> FilesOf(const QMimeData* data) {
  assert(data != nullptr);
  std::vector<QString> files;
  for (const QUrl& url : data->urls()) {
    const bool is_file = url.isLocalFile();
    if (is_file) {
      files.push_back(url.toLocalFile());
    }
  }
  assert(files.size() <= static_cast<size_t>(data->urls().size()));
  return files;
}

}  // namespace

void ReelTimeline::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  const QMimeData* dropped = event->mimeData();
  assert(dropped != nullptr);
  const bool is_ours =
      ShotOfDrop(dropped).IsValid() || !FilesOf(dropped).empty();
  if (is_ours) {
    event->acceptProposedAction();
  }
}

void ReelTimeline::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  const QPointF at = event->position();
  const ShotId shot = ShotOfDrop(event->mimeData());
  const Shot* found = shot.IsValid() ? FindShot(project, shot) : nullptr;
  drop_track_ = TrackAt(at.y());
  drop_frame_ = SnapFrame(project, FrameAt(at.x()), SnapReach(), {},
                          managers_.playback->FrameOn(Timeline::kReel));
  drop_length_ = found != nullptr ? found->length : Frame(kFileDropFrames);
  const bool can_land = drop_track_ >= 0;
  if (can_land) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
  update();
}

void ReelTimeline::dragLeaveEvent(QDragLeaveEvent* event) {
  assert(event != nullptr);
  drop_track_ = -1;
  update();
  assert(drop_track_ == -1);
}

void ReelTimeline::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(managers_.reel != nullptr);
  const int track = drop_track_;
  Frame at = drop_frame_;
  drop_track_ = -1;
  update();
  const bool is_on_track = track >= 0;
  if (!is_on_track) {
    return;
  }
  event->acceptProposedAction();
  const ShotId shot = ShotOfDrop(event->mimeData());
  std::set<ClipId> added;
  const bool is_shot_drop = shot.IsValid();
  if (is_shot_drop) {
    const auto clip = managers_.reel->AddShot(shot, track, at);
    Report(ProblemOf(clip));
    if (clip) {
      added.insert(*clip);
    }
  }
  // Several files land one after another.
  for (const QString& file : FilesOf(event->mimeData())) {
    const auto clip = managers_.reel->AddVideo(file, track, at);
    Report(ProblemOf(clip));
    if (!clip) {
      break;
    }
    added.insert(*clip);
    at = ClipOf(managers_.history->current().reel, *clip)->end();
  }
  managers_.selection->PickClips(added, PickMode::kReplace);
}

void ReelTimeline::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(zoom_ > 0.0);
  const QPoint turn = event->angleDelta();
  const double notches =
      (turn.y() != 0 ? turn.y() : turn.x()) / kWheelUnit;
  const bool is_zoom = event->modifiers().testFlag(Qt::ControlModifier);
  if (is_zoom) {
    // The frame under the mouse stays put.
    const double x = event->position().x();
    const double under = (x - kHeaderWidth) / zoom_ + scroll_;
    SetZoom(zoom_ * std::pow(1.25, notches));
    scroll_ = std::max(0.0, under - (x - kHeaderWidth) / zoom_);
  } else {
    scroll_ = std::max(0.0, scroll_ - notches * kScrollPixels / zoom_);
  }
  event->accept();
  update();
}

}  // namespace snapper
