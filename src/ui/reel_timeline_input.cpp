// ReelTimeline's mouse: picking, moving, trimming and seeking.

#include <QFileDialog>
#include <QMouseEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <set>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/reel_timeline.h"
#include "ui/slot_picker.h"

namespace snapper {
namespace {

constexpr double kEdgeReach = 6.0;
constexpr double kSnapPixels = 8.0;
// Pixels the mouse goes before a press on a clip becomes a drag.
constexpr double kDragStart = 3.0;

}  // namespace

ClipId ReelTimeline::ClipAt(QPointF at) const {
  assert(managers_.history != nullptr);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  for (const ReelTrack& track : managers_.history->current().reel.tracks) {
    for (const Clip& clip : track.clips) {
      const bool is_hit = ClipRect(clip.id).contains(at);
      if (is_hit) {
        return clip.id;
      }
    }
  }
  return ClipId();
}

ReelTimeline::Drag ReelTimeline::DragFor(ClipId clip, QPointF at) const {
  assert(clip.IsValid());
  const QRectF box = ClipRect(clip);
  assert(!box.isEmpty());
  // Narrow clips keep a middle to move by.
  const double reach = std::min(kEdgeReach, box.width() / 3.0);
  const bool is_left = at.x() - box.left() <= reach;
  const bool is_right = box.right() - at.x() <= reach;
  return is_left ? Drag::kTrimStart
         : is_right ? Drag::kTrimEnd
                    : Drag::kMove;
}

std::vector<ClipId> ReelTimeline::Picked() const {
  assert(managers_.selection != nullptr);
  const auto& clips = managers_.selection->clips();
  assert(clips.size() < 1000000);
  return std::vector<ClipId>(clips.begin(), clips.end());
}

void ReelTimeline::Pick(ClipId clip, Qt::KeyboardModifiers modifiers) {
  assert(clip.IsValid());
  assert(managers_.selection != nullptr);
  const bool is_shift = modifiers.testFlag(Qt::ShiftModifier);
  const bool is_ctrl = modifiers.testFlag(Qt::ControlModifier);
  const bool is_picked = managers_.selection->clips().contains(clip);
  // A plain click on a picked clip keeps the pick, so the pick drags.
  const PickMode mode = is_ctrl    ? PickMode::kToggle
                        : is_shift ? PickMode::kAdd
                        : is_picked ? PickMode::kAdd
                                    : PickMode::kReplace;
  managers_.selection->PickClips({clip}, mode);
}

int ReelTimeline::SnapReach() const {
  assert(zoom_ > 0.0);
  const int reach =
      std::max(0, static_cast<int>(std::lround(kSnapPixels / zoom_)));
  assert(reach >= 0);
  return reach;
}

void ReelTimeline::SeekTo(double x) {
  assert(std::isfinite(x));
  assert(managers_.playback->timeline() == Timeline::kReel || !isVisible());
  managers_.playback->Seek(FrameAt(x));
}

void ReelTimeline::StartDrag(ClipId clip, Drag drag, QPointF at) {
  assert(clip.IsValid());
  assert(drag != Drag::kNone && drag != Drag::kSeek);
  drag_ = drag;
  dragged_ = clip;
  press_ = at;
  has_moved_ = false;
  const bool is_move = drag == Drag::kMove;
  const QString label = !is_move               ? tr("Trim clip")
                        : Picked().size() > 1 ? tr("Move clips")
                                              : tr("Move clip");
  scope_ = std::make_unique<EditScope>(managers_.history, label);
}

void ReelTimeline::Follow(QPointF at) {
  assert(drag_ != Drag::kNone);
  assert(scope_ != nullptr);
  const Project& start = managers_.history->before();
  const Frame playhead = managers_.playback->FrameOn(Timeline::kReel);
  const bool is_still = !has_moved_ &&
                        std::abs(at.x() - press_.x()) < kDragStart &&
                        std::abs(at.y() - press_.y()) < kDragStart;
  if (is_still) {
    return;
  }
  has_moved_ = true;
  const bool is_move = drag_ == Drag::kMove;
  if (is_move) {
    const std::vector<ClipId> picked = Picked();
    const int raw =
        static_cast<int>(std::lround((at.x() - press_.x()) / zoom_));
    const int frames =
        SnapDelta(start, std::set<ClipId>(picked.begin(), picked.end()), raw,
                  SnapReach(), playhead);
    // Upwards is a higher track.
    const int tracks =
        static_cast<int>(std::lround((press_.y() - at.y()) / kTrackHeight));
    Report(ProblemOf(managers_.reel->MoveAll(picked, tracks, frames)));
    return;
  }
  const Frame edge = SnapFrame(start, FrameAt(at.x() + zoom_ / 2.0),
                               SnapReach(), {dragged_}, playhead);
  const bool is_start = drag_ == Drag::kTrimStart;
  Report(ProblemOf(is_start ? managers_.reel->TrimStart(dragged_, edge)
                            : managers_.reel->TrimEnd(dragged_, edge)));
}

void ReelTimeline::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const QPointF at = event->position();
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  setFocus();
  const bool is_ruler = at.y() < kRulerHeight && at.x() >= kHeaderWidth;
  if (is_ruler) {
    drag_ = Drag::kSeek;
    SeekTo(at.x());
    return;
  }
  const ClipId clip = ClipAt(at);
  const bool is_on_clip = clip.IsValid();
  if (!is_on_clip) {
    const bool is_plain = event->modifiers() == Qt::NoModifier;
    if (is_plain) {
      managers_.selection->PickClips({}, PickMode::kReplace);
    }
    return;
  }
  is_plain_press_ = event->modifiers() == Qt::NoModifier;
  Pick(clip, event->modifiers());
  const bool is_still_picked = managers_.selection->clips().contains(clip);
  if (is_still_picked) {
    StartDrag(clip, DragFor(clip, at), at);
  }
}

void ReelTimeline::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.playback != nullptr);
  const QPointF at = event->position();
  const ClipId hover = ClipAt(at);
  const bool is_edge =
      hover.IsValid() && DragFor(hover, at) != Drag::kMove;
  const bool is_trimming =
      drag_ == Drag::kTrimStart || drag_ == Drag::kTrimEnd;
  setCursor(is_edge || is_trimming ? Qt::SizeHorCursor : Qt::ArrowCursor);
  const bool is_seeking = drag_ == Drag::kSeek;
  if (is_seeking) {
    SeekTo(at.x());
    return;
  }
  const bool is_dragging = drag_ != Drag::kNone;
  if (is_dragging) {
    Follow(at);
  }
}

void ReelTimeline::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_click =
      drag_ == Drag::kMove && !has_moved_ && is_plain_press_;
  // Leaving the scope lands the drag as one step.
  scope_.reset();
  if (was_click) {
    managers_.selection->PickClips({dragged_}, PickMode::kReplace);
  }
  drag_ = Drag::kNone;
  dragged_ = ClipId();
  has_moved_ = false;
  assert(scope_ == nullptr);
}

void ReelTimeline::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const QPointF at = event->position();
  const int track = TrackAt(at.y());
  const bool is_on_track = event->button() == Qt::LeftButton &&
                           track >= 0 && at.x() >= kHeaderWidth;
  if (!is_on_track) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }
  // The first click may have opened a drag; the gap is picked instead.
  scope_.reset();
  drag_ = Drag::kNone;
  FillGap(track, FrameAt(at.x()));
}

void ReelTimeline::FillGap(int track, Frame at) {
  assert(track >= 0);
  assert(managers_.reel != nullptr);
  const Project& project = managers_.history->current();
  const Slot slot = SlotAt(project.reel, at);
  const bool is_gap = slot.IsValid();
  if (!is_gap) {
    Report(tr("Mark a cut on both sides of the gap first (M)."));
    return;
  }
  std::optional<SlotFill> fill = SlotFillAt(project, track, at);
  const bool has_video = fill.has_value();
  if (!has_video) {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Fill the gap from"), QString(),
        tr("Videos (*.mp4 *.mov *.mkv *.webm *.avi *.m4v);;All files (*)"));
    const bool is_cancelled = path.isEmpty();
    if (is_cancelled) {
      return;
    }
    const auto video = managers_.reel->ReadVideo(path);
    if (!video) {
      Report(video.error().message);
      return;
    }
    fill = SlotFill{*video, Frame(0)};
  }
  SlotPicker picker(managers_, slot, *fill, this);
  const bool is_chosen = picker.exec() == QDialog::Accepted;
  if (is_chosen) {
    Report(ProblemOf(managers_.reel->FillSlot(track, at, picker.source(),
                                              picker.in())));
  }
}

}  // namespace snapper
