// ReelTimeline's mouse: picking, moving, trimming and seeking.

#include <QFileDialog>
#include <QMenu>
#include <QMouseEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <set>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/drag_text.h"
#include "ui/form_helpers.h"
#include "ui/grab.h"
#include "ui/pick_keys.h"
#include "ui/reel_timeline.h"
#include "ui/slot_picker.h"
#include "ui/transition_picker.h"

namespace snapper {
namespace {

constexpr double kSnapPixels = 8.0;

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
        std::abs(XOf(clip.end()) - at.x()) <= kGrabReach &&
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
  const double reach = std::min(kGrabReach, box.width() / 3.0);
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
  const bool is_picked = managers_.selection->clips().contains(clip);
  // A plain click on a picked clip keeps the pick, so the pick drags.
  const std::optional<PickMode> mode = ClickMode(modifiers, is_picked);
  if (mode.has_value()) {
    managers_.selection->PickClips({clip}, *mode);
  }
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

void ReelTimeline::StartDrag(ClipId clip, Drag drag, QPointF at,
                             Qt::KeyboardModifiers modifiers) {
  assert(clip.IsValid());
  assert(drag != Drag::kNone && drag != Drag::kSeek);
  drag_ = drag;
  dragged_ = clip;
  press_ = at;
  has_moved_ = false;
  drag_playhead_.Begin();
  const bool is_move = drag == Drag::kMove;
  const bool is_alt = modifiers.testFlag(Qt::AltModifier);
  is_resize_ = !is_move && managers_.reel->IsShotResize(clip, is_alt);
  is_roll_ = !is_move && managers_.reel->IsCueRoll(clip, is_alt);
  const QString label = is_roll_     ? tr("Move cue")
                        : is_resize_ ? managers_.reel->ResizeLabel(Picked(), clip)
                        : !is_move ? tr("Trim clip")
                        : Picked().size() > 1 ? tr("Move clips")
                                              : tr("Move clip");
  scope_ = std::make_unique<EditScope>(managers_.history, label);
  GrabGhost();
}

void ReelTimeline::Follow(QPointF at, Qt::KeyboardModifiers modifiers) {
  assert(drag_ != Drag::kNone);
  assert(scope_ != nullptr);
  const Project& start = managers_.history->before();
  // Snaps go to where the playhead was, not to where the drag put it.
  const Frame playhead = drag_playhead_.origin();
  const bool is_still = !has_moved_ && !IsPastDeadZone(press_, at);
  if (is_still) {
    return;
  }
  has_moved_ = true;
  const bool is_free = modifiers.testFlag(Qt::ControlModifier);
  const int reach = is_free ? 0 : SnapReach();
  const bool is_move = drag_ == Drag::kMove;
  if (is_move) {
    const std::vector<ClipId> picked = Picked();
    const int raw =
        static_cast<int>(std::lround((at.x() - press_.x()) / zoom_));
    const int frames =
        SnapDelta(start, std::set<ClipId>(picked.begin(), picked.end()), raw,
                  reach, playhead);
    // Upwards is a higher track.
    const int tracks =
        static_cast<int>(std::lround((press_.y() - at.y()) / kTrackHeight));
    Report(ProblemOf(managers_.reel->MoveAll(picked, tracks, frames)));
    feedback_.Update(press_, at, FramesText(frames), frames != raw);
    const Clip* grabbed = ClipOf(start.reel, dragged_);
    const bool has_grabbed = grabbed != nullptr;
    if (has_grabbed) {
      CarryPlayhead(Frame(std::max(0, grabbed->start.index() + frames)));
    }
    return;
  }
  const Frame raw = FrameAt(at.x() + zoom_ / 2.0);
  const Frame edge = SnapFrame(start, raw, reach, {dragged_}, playhead);
  // Edges push or pull the clips after them; Shift leaves them put.
  const bool is_ripple = !modifiers.testFlag(Qt::ShiftModifier);
  if (is_resize_) {
    FollowResize(raw, edge, at, is_ripple);
    return;
  }
  const bool is_start = drag_ == Drag::kTrimStart;
  ReelManager& reel = *managers_.reel;
  const bool is_ripple_trim = is_ripple && !is_roll_;
  if (is_ripple_trim) {
    FollowRippleTrim(raw, edge, at);
    return;
  }
  Report(ProblemOf(is_roll_ ? (is_start ? reel.RollCueStart(dragged_, edge)
                                        : reel.RollCueEdge(dragged_, edge))
                   : is_start ? reel.TrimStart(dragged_, edge)
                              : reel.TrimEnd(dragged_, edge)));
  ShowTrim(raw, edge, at);
  CarryPlayhead(edge);
}

void ReelTimeline::FollowRippleTrim(Frame raw, Frame edge, QPointF at) {
  assert(drag_ == Drag::kTrimStart || drag_ == Drag::kTrimEnd);
  assert(managers_.reel != nullptr);
  const bool is_start = drag_ == Drag::kTrimStart;
  Report(ProblemOf(managers_.reel->RippleTrim(dragged_, is_start, edge)));
  ShowTrim(raw, edge, at);
  const Clip* clip = ClipOf(managers_.history->current().reel, dragged_);
  const bool has_clip = clip != nullptr;
  if (has_clip) {
    // A start trim keeps the clip's start: its new first frame is there.
    CarryPlayhead(is_start ? clip->start : edge);
  }
}

void ReelTimeline::FollowResize(Frame raw, Frame edge, QPointF at,
                                bool is_ripple) {
  assert(is_resize_);
  assert(managers_.reel != nullptr);
  const bool is_start = drag_ == Drag::kTrimStart;
  const auto resized =
      managers_.reel->ResizeShots(Picked(), dragged_, is_start, edge, is_ripple);
  if (!resized) {
    Report(resized.error().message);
    return;
  }
  // The playhead stays on its reel frame: a resize does not carry it.
  feedback_.Update(press_, at,
                   ShotLengthText(resized->length.index(), resized->delta,
                                  resized->stopped),
                   edge.index() != raw.index());
}

void ReelTimeline::CarryPlayhead(Frame landing) {
  assert(landing.index() >= 0);
  assert(drag_playhead_.is_active());
  managers_.playback->Seek(landing);
}

void ReelTimeline::ShowTrim(Frame raw, Frame edge, QPointF at) {
  assert(drag_ == Drag::kTrimStart || drag_ == Drag::kTrimEnd);
  assert(managers_.history != nullptr);
  const Clip* clip = ClipOf(managers_.history->before().reel, dragged_);
  const bool has_clip = clip != nullptr;
  if (!has_clip) {
    return;
  }
  const Frame was = drag_ == Drag::kTrimStart ? clip->start : clip->end();
  feedback_.Update(press_, at, FramesText(edge.index() - was.index()),
                   edge.index() != raw.index());
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
  const bool is_header = PressSoundHeader(at);
  if (is_header) {
    return;
  }
  const ClipId badge = BadgeAt(at);
  const bool is_badge = badge.IsValid();
  if (is_badge) {
    EditTransition(badge);
    return;
  }
  const ClipId clip = ClipAt(at);
  const bool is_on_clip = clip.IsValid();
  if (!is_on_clip) {
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int track = TrackAt(at.y());
    // Alt marks a range on the track for New shot here.
    const bool is_mark = mods.testFlag(Qt::AltModifier) && track >= 0 &&
                         at.x() >= kHeaderWidth;
    press_ = at;
    box_to_ = at;
    if (is_mark) {
      drag_ = Drag::kMark;
      marked_track_ = track;
      marked_ = Slot{FrameAt(at.x()), FrameAt(at.x())};
      update();
      return;
    }
    // Empty space starts a box; Shift adds, Ctrl takes out.
    drag_ = Drag::kBox;
    box_mode_ = BoxMode(mods);
    return;
  }
  is_plain_press_ = event->modifiers() == Qt::NoModifier;
  // A sound clip's fade handles and volume line come before its ends.
  const Drag sound = SoundDragFor(clip, at);
  const bool is_sound_drag = sound != Drag::kNone;
  if (is_sound_drag) {
    managers_.selection->PickClips({clip}, PickMode::kReplace);
    StartSoundDrag(clip, sound, at);
    return;
  }
  Pick(clip, event->modifiers());
  PreferPicked();
  const bool is_still_picked = managers_.selection->clips().contains(clip);
  if (is_still_picked) {
    StartDrag(clip, DragFor(clip, at), at, event->modifiers());
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
  const Drag side = is_edge ? DragFor(hover, at) : Drag::kNone;
  const bool is_free = drag_ == Drag::kNone;
  if (is_free) {
    const bool has_side = side != Drag::kNone;
    hover_.Set(has_side ? GrabKind::kEdge : GrabKind::kNone,
               has_side ? EdgeKey(hover, side == Drag::kTrimStart)
                        : QString());
  }
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
  const bool is_marking = drag_ == Drag::kMark;
  if (is_marking) {
    const Frame from = FrameAt(press_.x());
    const Frame to = FrameAt(at.x());
    marked_ = Slot{std::min(from, to), Frame(std::max(from, to).index() + 1)};
    feedback_.Update(press_, at, FramesText(marked_.length().index()), false);
    update();
    return;
  }
  const bool is_sound_drag = drag_ == Drag::kVolume ||
                             drag_ == Drag::kFadeIn ||
                             drag_ == Drag::kFadeOut || drag_ == Drag::kFader;
  if (is_sound_drag) {
    FollowSound(at);
    return;
  }
  const bool is_dragging = drag_ != Drag::kNone;
  if (is_dragging) {
    Follow(at, event->modifiers());
  }
}

void ReelTimeline::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_boxing = drag_ == Drag::kBox;
  if (is_boxing) {
    FinishBox();
  }
  // An Alt click without a drag marks nothing.
  const bool is_short_mark =
      drag_ == Drag::kMark && !IsPastDeadZone(press_, event->position());
  if (is_short_mark) {
    marked_ = Slot{};
    marked_track_ = -1;
    update();
  }
  const bool was_click =
      drag_ == Drag::kMove && !has_moved_ && is_plain_press_;
  // Leaving the scope lands the drag as one step.
  feedback_.Settle();
  is_landing_drag_ = true;
  scope_.reset();
  is_landing_drag_ = false;
  drag_playhead_.End();
  if (was_click) {
    managers_.selection->PickClips({dragged_}, PickMode::kReplace);
  }
  drag_ = Drag::kNone;
  dragged_ = ClipId();
  drag_track_ = -1;
  has_moved_ = false;
  is_resize_ = false;
  is_roll_ = false;
  assert(scope_ == nullptr);
}

void ReelTimeline::FinishBox() {
  assert(drag_ == Drag::kBox);
  assert(managers_.selection != nullptr);
  drag_ = Drag::kNone;
  update();
  const QRectF box = QRectF(press_, box_to_).normalized();
  const bool is_click =
      box.width() < kDragDeadZone && box.height() < kDragDeadZone;
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
  const bool is_left = event->button() == Qt::LeftButton;
  const bool is_on_track = is_left && track >= 0 && at.x() >= kHeaderWidth;
  const bool is_sound_track =
      track >= 0 &&
      managers_.history->current().reel.tracks[static_cast<size_t>(track)]
          .is_sound;
  if (is_left && is_sound_track) {
    // The first click opened a drag; a double click resets instead.
    feedback_.Cancel();
    if (scope_ != nullptr) {
      scope_->Cancel();
    }
    scope_.reset();
    drag_ = Drag::kNone;
    ResetSoundAt(at);
    return;
  }
  if (!is_on_track) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }
  // The first click may have opened a drag; this picks instead.
  feedback_.Cancel();
  scope_.reset();
  drag_ = Drag::kNone;
  const ClipId joint = JointAt(at);
  const bool is_joint = joint.IsValid();
  if (is_joint) {
    EditTransition(joint);
    return;
  }
  const ClipId clip = ClipAt(at);
  const Clip* found = clip.IsValid()
                          ? ClipOf(managers_.history->current().reel, clip)
                          : nullptr;
  const bool is_text = found != nullptr && IsTextClip(*found);
  if (is_text) {
    emit TextEditAsked(clip);
    return;
  }
  const bool is_subtitle_track =
      track == SubtitleTrack(managers_.history->current().reel);
  if (is_subtitle_track) {
    return;
  }
  const auto* shot =
      found != nullptr ? std::get_if<ShotSource>(&found->source) : nullptr;
  if (shot != nullptr) {
    // Opens the shot on the Pose tab at the frame under the click.
    const Frame inside(found->in.index() + FrameAt(at.x()).index() -
                       found->start.index());
    emit OpenShotAsked(shot->shot, inside);
    return;
  }
  const bool is_on_clip = found != nullptr;
  if (is_on_clip) {
    FillGap(track, FrameAt(at.x()));
    return;
  }
  OfferEmpty(track, FrameAt(at.x()), event->globalPosition().toPoint());
}

void ReelTimeline::OfferEmpty(int track, Frame at, QPoint where) {
  assert(track >= 0);
  assert(managers_.reel != nullptr);
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  const Slot span = NewShotSpanAt(track, at);
  QAction* new_shot = menu.addAction(
      span.IsValid() ? tr("New shot here (%1)").arg(TimeText(span.length()))
                     : tr("New shot here"));
  Explain(new_shot,
          managers_.reel->WhyNoNewShot(track, span.start, span.end));
  const bool is_gap = SlotAt(managers_.history->current().reel, at).IsValid();
  QAction* fill = menu.addAction(tr("Pick what plays here..."));
  Explain(fill, is_gap ? QString()
                       : tr("Mark a cut on both sides of the gap first (M)."));
  const QAction* chosen = menu.exec(where);
  if (chosen == new_shot) {
    NewShotHere(track, at);
  } else if (chosen == fill) {
    FillGap(track, at);
  }
}

Slot ReelTimeline::NewShotSpanAt(int track, Frame at) const {
  assert(track >= -1);
  assert(managers_.reel != nullptr);
  const bool is_in_mark = marked_.IsValid() && marked_track_ == track &&
                          marked_.start <= at && at < marked_.end;
  return is_in_mark ? marked_ : managers_.reel->NewShotSpan(track, at);
}

void ReelTimeline::NewShotHere(int track, Frame at) {
  assert(track >= -1);
  assert(managers_.selection != nullptr);
  const Slot span = NewShotSpanAt(track, at);
  const auto made = managers_.reel->AddNewShot(track, span.start, span.end);
  Report(ProblemOf(made));
  if (!made) {
    return;
  }
  marked_ = Slot{};
  marked_track_ = -1;
  managers_.selection->PickClips({*made}, PickMode::kReplace);
}

void ReelTimeline::NewShotAtPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.reel != nullptr);
  const bool has_mark = marked_.IsValid() && marked_track_ >= 0;
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const Slot span =
      has_mark ? marked_ : managers_.reel->NewShotSpan(-1, now);
  const auto made = managers_.reel->AddNewShot(
      has_mark ? marked_track_ : -1, span.start, span.end);
  Report(ProblemOf(made));
  if (!made) {
    return;
  }
  marked_ = Slot{};
  marked_track_ = -1;
  managers_.selection->PickClips({*made}, PickMode::kReplace);
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
