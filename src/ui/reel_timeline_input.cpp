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
#include "ui/transition_picker.h"

namespace snapper {
namespace {

// Ends are drawn 3 pixels wide but grab from further in.
constexpr double kEdgeReach = 8.0;
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

ClipId ReelTimeline::JointAt(QPointF at) const {
  assert(managers_.history != nullptr);
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  const int track = TrackAt(at.y());
  const bool is_on_track = track >= 0 && at.x() >= kHeaderWidth;
  if (!is_on_track) {
    return ClipId();
  }
  const ReelTrack& row =
      managers_.history->current().reel.tracks[static_cast<size_t>(track)];
  for (const Clip& clip : row.clips) {
    const bool is_near =
        std::abs(XOf(clip.end()) - at.x()) <= kEdgeReach &&
        NextTouching(row, clip) != nullptr;
    if (is_near) {
      return clip.id;
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
    // Empty space starts a box; Shift adds, Ctrl takes out.
    const Qt::KeyboardModifiers mods = event->modifiers();
    drag_ = Drag::kBox;
    press_ = at;
    box_to_ = at;
    box_mode_ = mods.testFlag(Qt::ShiftModifier)     ? PickMode::kAdd
                : mods.testFlag(Qt::ControlModifier) ? PickMode::kRemove
                                                     : PickMode::kReplace;
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
  const bool is_joint = JointAt(at).IsValid();
  setToolTip(is_joint ? tr("Double-click to pick the transition here.")
                      : QString());
  const bool is_seeking = drag_ == Drag::kSeek;
  if (is_seeking) {
    SeekTo(at.x());
    return;
  }
  const bool is_boxing = drag_ == Drag::kBox;
  if (is_boxing) {
    box_to_ = at;
    update();
    return;
  }
  const bool is_dragging = drag_ != Drag::kNone;
  if (is_dragging) {
    Follow(at);
  }
}

void ReelTimeline::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_boxing = drag_ == Drag::kBox;
  if (is_boxing) {
    FinishBox();
  }
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

void ReelTimeline::FinishBox() {
  assert(drag_ == Drag::kBox);
  assert(managers_.selection != nullptr);
  drag_ = Drag::kNone;
  update();
  const QRectF box = QRectF(press_, box_to_).normalized();
  const bool is_click = box.width() < kDragStart && box.height() < kDragStart;
  if (is_click) {
    const bool is_plain = box_mode_ == PickMode::kReplace;
    if (is_plain) {
      managers_.selection->PickClips({}, PickMode::kReplace);
    }
    return;
  }
  const int count =
      static_cast<int>(managers_.history->current().reel.tracks.size());
  // Above the tracks is the top one; below them, the bottom one.
  const int high = box.top() < kRulerHeight ? count - 1 : TrackAt(box.top());
  const int low = TrackAt(box.bottom());
  const std::set<ClipId> caught = ClipsIn(
      managers_.history->current().reel, low < 0 ? 0 : low,
      high, FrameAt(std::max(box.left(), kHeaderWidth)),
      FrameAt(std::max(box.right(), kHeaderWidth)));
  managers_.selection->PickClips(caught, box_mode_);
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
  // The first click may have opened a drag; this picks instead.
  scope_.reset();
  drag_ = Drag::kNone;
  const ClipId joint = JointAt(at);
  const bool is_joint = joint.IsValid();
  if (is_joint) {
    EditTransition(joint);
    return;
  }
  FillGap(track, FrameAt(at.x()));
}

void ReelTimeline::EditTransition(ClipId clip) {
  assert(clip.IsValid());
  assert(managers_.reel != nullptr);
  const QString why_not = managers_.reel->WhyNoTransition(clip);
  const bool can_edit = why_not.isEmpty();
  if (!can_edit) {
    Report(why_not);
    return;
  }
  const Reel& reel = managers_.history->current().reel;
  const ReelTrack& track =
      reel.tracks[static_cast<size_t>(FindClip(reel, clip).track)];
  const Clip& left = *ClipOf(reel, clip);
  TransitionPicker picker(left.out, LongestTransition(track, left), this);
  const bool is_chosen = picker.exec() == QDialog::Accepted;
  if (is_chosen) {
    Report(ProblemOf(
        managers_.reel->SetTransition(clip, picker.transition())));
  }
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
