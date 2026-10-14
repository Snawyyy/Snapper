// ReelTimeline's drops and wheel.

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
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
#include "edit/sound_manager.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"

namespace snapper {
namespace {

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
  // Sound files go to a sound track: the one dropped on, else the first
  // with room at the drop point; in a row, as one step.
  QStringList sounds;
  std::vector<QString> videos;
  for (const QString& file : FilesOf(event->mimeData())) {
    // Subtitle files land on the subtitle track, wherever dropped.
    const QString suffix = QFileInfo(file).suffix().toLower();
    const bool is_subtitles = suffix == "srt" || suffix == "vtt";
    if (is_subtitles) {
      const auto made = managers_.reel->ImportSubtitles(file);
      Report(ProblemOf(made));
      if (made) {
        added.insert(made->begin(), made->end());
      }
      continue;
    }
    if (IsSoundFile(file)) {
      sounds.append(file);
    } else {
      videos.push_back(file);
    }
  }
  const bool has_sounds = !sounds.isEmpty();
  if (has_sounds) {
    const bool is_sound_track =
        managers_.history->current().reel.tracks[static_cast<size_t>(track)]
            .is_sound;
    const auto made =
        managers_.reel->AddSounds(sounds, is_sound_track ? track : -1, at);
    Report(ProblemOf(made));
    if (made) {
      added.insert(made->begin(), made->end());
    }
  }
  // Several files land one after another.
  for (const QString& file : videos) {
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
  assert(zoom() > 0.0);
  // The wheel over the track names scrolls the tracks, as Shift does
  // anywhere; elsewhere it is the shared rule: plain scrolls time, Ctrl
  // zooms round the cursor.
  const bool is_over_names = event->position().x() < kHeaderWidth;
  const bool is_zoom = event->modifiers().testFlag(Qt::ControlModifier);
  const WheelUse use = is_over_names && !is_zoom ? WheelUse::kRows
                                                 : scale_.Wheel(*event);
  const bool is_tracks = use == WheelUse::kRows;
  if (is_tracks) {
    const double notches = TimelineScale::Notches(*event);
    track_scroll_.setValue(track_scroll_.value() -
                           static_cast<int>(notches * kTrackHeight / 2.0));
  }
  event->accept();
  update();
}

}  // namespace snapper
