// ReelTimeline's drops, menus, keys and wheel.

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMenu>
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

// Points action at why it can't act, greyed out, or enables it.
void Explain(QAction* action, const QString& why_not) {
  assert(action != nullptr);
  assert(why_not.size() < 100000);
  action->setEnabled(why_not.isEmpty());
  action->setToolTip(why_not);
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

void ReelTimeline::SplitPicked() {
  assert(managers_.reel != nullptr);
  assert(managers_.playback != nullptr);
  const auto halves = managers_.reel->SplitAll(
      Picked(), managers_.playback->FrameOn(Timeline::kReel));
  Report(ProblemOf(halves));
}

void ReelTimeline::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const ClipId clip = ClipAt(event->pos());
  const bool is_on_clip = clip.IsValid();
  if (is_on_clip) {
    // Right-clicking outside the pick picks just that clip first.
    const bool is_picked = managers_.selection->clips().contains(clip);
    if (!is_picked) {
      managers_.selection->PickClips({clip}, PickMode::kReplace);
    }
    ClipMenu(event->globalPos());
    return;
  }
  const int track = TrackAt(event->pos().y());
  const bool is_on_track = track >= 0;
  if (is_on_track) {
    TrackMenu(track, event->globalPos());
  }
}

void ReelTimeline::ClipMenu(QPoint where) {
  assert(managers_.reel != nullptr);
  assert(!managers_.selection->clips().empty());
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  QAction* split = menu.addAction(tr("Split at playhead"));
  split->setShortcut(Qt::Key_S);
  Explain(split, managers_.reel->WhyNoSplit(
                     Picked(), managers_.playback->FrameOn(Timeline::kReel)));
  connect(split, &QAction::triggered, this, [this] { SplitPicked(); });
  QAction* remove = menu.addAction(tr("Remove"));
  remove->setShortcut(Qt::Key_Delete);
  connect(remove, &QAction::triggered, this, [this] {
    Report(ProblemOf(managers_.reel->RemoveAll(Picked())));
  });
  menu.exec(where);
}

void ReelTimeline::TrackMenu(int track, QPoint where) {
  assert(track >= 0);
  assert(managers_.reel != nullptr);
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  QAction* add = menu.addAction(tr("Add track on top"));
  Explain(add, managers_.reel->WhyNoAddTrack());
  connect(add, &QAction::triggered, this,
          [this] { Report(ProblemOf(managers_.reel->AddTrack())); });
  QAction* remove = menu.addAction(tr("Remove track V%1").arg(track + 1));
  Explain(remove, managers_.reel->WhyNoRemoveTrack(track));
  connect(remove, &QAction::triggered, this, [this, track] {
    Report(ProblemOf(managers_.reel->RemoveTrack(track)));
  });
  menu.exec(where);
}

void ReelTimeline::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.reel != nullptr);
  const int key = event->key();
  const bool is_cancel = key == Qt::Key_Escape && scope_ != nullptr;
  const bool is_unpick = key == Qt::Key_Escape && scope_ == nullptr;
  const bool is_delete = key == Qt::Key_Delete || key == Qt::Key_Backspace;
  const bool is_split = key == Qt::Key_S && event->modifiers() == 0;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (is_cancel) {
    scope_->Cancel();
    scope_.reset();
    drag_ = Drag::kNone;
  } else if (is_unpick) {
    managers_.selection->PickClips({}, PickMode::kReplace);
  } else if (is_delete) {
    Report(ProblemOf(managers_.reel->RemoveAll(Picked())));
  } else if (is_split) {
    SplitPicked();
  } else if (is_all) {
    std::set<ClipId> all;
    for (const ReelTrack& track : managers_.history->current().reel.tracks) {
      for (const Clip& clip : track.clips) {
        all.insert(clip.id);
      }
    }
    managers_.selection->PickClips(all, PickMode::kReplace);
  } else {
    QWidget::keyPressEvent(event);
  }
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
