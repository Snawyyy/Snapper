// CompTimeline: the effect clips, and dragging them.

#include <QKeyEvent>
#include <QMouseEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_timeline.h"

namespace snapper {
namespace {

// How near an end grabs it, and how near a snap catches, in pixels.
constexpr double kEdge = 6.0;
constexpr double kSnap = 6.0;

}  // namespace

const Clip* CompTimeline::ClipOf(ShotId shot) const {
  assert(managers_.history != nullptr);
  assert(shot.value() >= 0);
  const Clip* first = nullptr;
  for (const ReelTrack& track : managers_.history->current().reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      const bool is_earlier =
          source != nullptr && source->shot == shot &&
          (first == nullptr || clip.start.index() < first->start.index());
      first = is_earlier ? &clip : first;
    }
  }
  return first;
}

std::vector<CompTimeline::Bar> CompTimeline::Bars() const {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  std::vector<std::pair<const Clip*, const Shot*>> shots;
  for (const auto& shot : project.shots) {
    const Clip* clip = ClipOf(shot->id);
    const bool is_on_reel = clip != nullptr && !shot->comp.empty();
    if (is_on_reel) {
      shots.emplace_back(clip, shot.get());
    }
  }
  std::ranges::sort(shots, [](const auto& a, const auto& b) {
    return a.first->start.index() < b.first->start.index();
  });
  std::vector<Bar> bars;
  double y = kRuler + kRow;
  for (const auto& [clip, shot] : shots) {
    for (const CompEffect& effect : shot->comp) {
      Bar bar;
      bar.shot = shot->id;
      bar.clip = clip;
      bar.offset = clip->start.index() - clip->in.index();
      bar.id = effect.id;
      bar.is_on_doll = effect.target.layer.IsValid();
      const bool is_whole = effect.length == kWholeShot;
      bar.first = is_whole ? clip->in.index() : effect.start;
      bar.end = is_whole ? clip->in.index() + clip->length.index()
                         : effect.start + effect.length;
      const double left = XOf(bar.first + bar.offset);
      const double right = XOf(bar.end + bar.offset);
      bar.box = QRectF(left, y + 1, std::max(4.0, right - left), kRow - 2);
      bars.push_back(bar);
      y += kRow;
    }
  }
  return bars;
}

int CompTimeline::Snapped(int frame) const {
  assert(managers_.playback != nullptr);
  assert(drag_clip_end_ >= drag_clip_start_);
  const int playhead = managers_.playback->FrameOn(Timeline::kReel).index();
  for (const int target : {drag_clip_start_, drag_clip_end_, playhead}) {
    const bool is_near = std::abs(XOf(target) - XOf(frame)) <= kSnap;
    if (is_near) {
      return target;
    }
  }
  return frame;
}

bool CompTimeline::StartDrag(QPointF at) {
  assert(managers_.comp != nullptr);
  assert(std::isfinite(at.x()));
  for (const Bar& bar : Bars()) {
    const bool is_on = bar.box.adjusted(-3, 0, 3, 0).contains(at);
    if (!is_on) {
      continue;
    }
    const bool is_start = at.x() <= bar.box.left() + kEdge;
    const bool is_end = at.x() >= bar.box.right() - kEdge;
    grab_ = is_start ? Grab::kStart : is_end ? Grab::kEnd : Grab::kMove;
    dragged_shot_ = bar.shot;
    dragged_ = bar.id;
    drag_offset_ = bar.offset;
    drag_clip_start_ = bar.clip->start.index();
    drag_clip_end_ = bar.clip->end().index();
    press_frame_ = FreeFrameAt(at.x());
    from_first_ = bar.first;
    from_end_ = bar.end;
    managers_.comp->Pick(bar.shot, bar.id);
    drag_ = std::make_unique<EditScope>(
        managers_.history,
        grab_ == Grab::kMove ? tr("Move effect") : tr("Trim effect"));
    return true;
  }
  return false;
}

void CompTimeline::DragTo(QPointF at, bool is_free) {
  assert(grab_ != Grab::kNone);
  assert(std::isfinite(at.x()));
  const int offset = drag_offset_;
  const int delta = FreeFrameAt(at.x()) - press_frame_;
  const auto snap = [&](int shot_frame) {
    return is_free ? shot_frame : Snapped(shot_frame + offset) - offset;
  };
  int first = from_first_;
  int end = from_end_;
  const bool is_move = grab_ == Grab::kMove;
  const bool is_start = grab_ == Grab::kStart;
  if (is_move) {
    const int moved = from_first_ + delta;
    const int by_start = snap(moved);
    const int by_end = snap(moved + from_end_ - from_first_) -
                       (from_end_ - from_first_);
    first = by_start != moved ? by_start : by_end;
    end = first + (from_end_ - from_first_);
  } else if (is_start) {
    first = std::min(from_end_ - 1, snap(from_first_ + delta));
  } else {
    end = std::max(from_first_ + 1, snap(from_end_ + delta));
  }
  static_cast<void>(managers_.comp->SetTiming(dragged_shot_, dragged_, first,
                                              end - first, is_move));
}

void CompTimeline::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  const QPointF at = event->position();
  const bool is_drag = at.x() >= kNameWidth && StartDrag(at);
  if (is_drag) {
    return;
  }
  for (const Bar& bar : Bars()) {
    const bool is_row = at.y() >= bar.box.top() - 1 &&
                        at.y() < bar.box.top() - 1 + kRow;
    if (is_row) {
      managers_.comp->Pick(bar.shot, bar.id);
    }
  }
  const bool is_on_frames = at.x() >= kNameWidth;
  if (is_on_frames) {
    SeekTo(at.x());
  }
}

void CompTimeline::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const QPointF at = event->position();
  const bool is_dragging = grab_ != Grab::kNone;
  if (is_dragging) {
    DragTo(at, event->modifiers().testFlag(Qt::ControlModifier));
    return;
  }
  const bool is_held = event->buttons().testFlag(Qt::LeftButton);
  if (is_held) {
    SeekTo(at.x());
    return;
  }
  Qt::CursorShape shape = Qt::ArrowCursor;
  for (const Bar& bar : Bars()) {
    const bool is_on = bar.box.adjusted(-3, 0, 3, 0).contains(at);
    const bool is_end = is_on && (at.x() <= bar.box.left() + kEdge ||
                                  at.x() >= bar.box.right() - kEdge);
    shape = is_end ? Qt::SizeHorCursor : is_on ? Qt::OpenHandCursor : shape;
  }
  setCursor(shape);
}

void CompTimeline::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  grab_ = Grab::kNone;
  // Leaving the scope lands the drag as one undo step.
  drag_.reset();
}

void CompTimeline::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  grab_ = Grab::kNone;
  drag_.reset();
  for (const Bar& bar : Bars()) {
    const bool is_on = bar.box.contains(event->position());
    if (is_on) {
      static_cast<void>(
          managers_.comp->SetTiming(bar.shot, bar.id, 0, kWholeShot, false));
      return;
    }
  }
}

void CompTimeline::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(grab_ == Grab::kNone || drag_ != nullptr);
  const bool is_cancel =
      event->key() == Qt::Key_Escape && drag_ != nullptr;
  if (is_cancel) {
    drag_->Cancel();
    drag_.reset();
    grab_ = Grab::kNone;
    return;
  }
  QWidget::keyPressEvent(event);
}

}  // namespace snapper
