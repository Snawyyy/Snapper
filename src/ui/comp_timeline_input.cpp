// CompTimeline: picking clips, the box, keys, the wheel and the menu.

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_timeline.h"
#include "ui/form_helpers.h"
#include "ui/pick_keys.h"
#include "ui/problem.h"

namespace snapper {

void CompTimeline::Report(const QString& problem) {
  assert(managers_.comp != nullptr);
  assert(problem.size() < 100000);
  const bool has_problem = !problem.isEmpty();
  if (has_problem) {
    emit Problem(problem);
  }
}

void CompTimeline::PressBar(const Bar& bar, QPointF at,
                            Qt::KeyboardModifiers mods) {
  assert(managers_.comp != nullptr);
  assert(bar.ref.id.IsValid());
  const bool is_picked = managers_.comp->ClipPicks().contains(bar.ref);
  const std::optional<PickMode> mode = ClickMode(mods, is_picked);
  const bool has_mode = mode.has_value();
  if (has_mode) {
    managers_.comp->PickClips({bar.ref}, *mode, bar.ref);
  }
  narrows_on_release_ = !has_mode;
  const bool is_now_picked = managers_.comp->ClipPicks().contains(bar.ref);
  if (is_now_picked) {
    StartDrag(bar, at);
  }
}

void CompTimeline::FinishBox(Qt::KeyboardModifiers mods) {
  assert(grab_ == Grab::kBox);
  assert(managers_.comp != nullptr);
  grab_ = Grab::kNone;
  if (!has_moved_) {
    // A click on empty rows: the playhead goes there, the pick goes.
    SeekTo(press_at_.x());
    const bool is_plain = mods == Qt::NoModifier;
    if (is_plain) {
      managers_.comp->PickClips({}, PickMode::kReplace, CompClipRef());
    }
    update();
    return;
  }
  const QRectF box = QRectF(press_at_, box_to_).normalized();
  std::set<CompClipRef> touched;
  for (const Bar& bar : Bars()) {
    const bool is_touched = box.intersects(bar.box);
    if (is_touched) {
      touched.insert(bar.ref);
    }
  }
  const CompClipRef focus =
      touched.empty() ? CompClipRef() : *touched.begin();
  managers_.comp->PickClips(touched, BoxMode(mods), focus);
  update();
}

void CompTimeline::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  const QPointF at = event->position();
  press_at_ = at;
  box_to_ = at;
  has_moved_ = false;
  const bool is_top = at.y() < kRuler + kRow;
  const bool is_frames = at.x() >= kNameWidth;
  const std::optional<Bar> bar = BarAt(at);
  const std::optional<Bar> row_bar =
      BarAt(QPointF(std::max(at.x(), kNameWidth), at.y()));
  const bool is_scrub = is_top && is_frames;
  const bool is_on_bar = bar.has_value() && is_frames;
  const bool is_name = !is_frames && row_bar.has_value();
  if (is_scrub) {
    grab_ = Grab::kScrub;
    SeekTo(at.x());
  } else if (is_on_bar) {
    PressBar(*bar, at, event->modifiers());
  } else if (is_name) {
    const bool is_picked =
        managers_.comp->ClipPicks().contains(row_bar->ref);
    const std::optional<PickMode> mode =
        ClickMode(event->modifiers(), is_picked);
    managers_.comp->PickClips({row_bar->ref},
                              mode.value_or(PickMode::kReplace),
                              row_bar->ref);
  } else if (is_frames) {
    grab_ = Grab::kBox;
  }
}

void CompTimeline::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const QPointF at = event->position();
  const bool is_free = event->modifiers().testFlag(Qt::ControlModifier);
  switch (grab_) {
    case Grab::kScrub:
      SeekTo(at.x());
      return;
    case Grab::kBox:
      box_to_ = at;
      has_moved_ = has_moved_ || (at - press_at_).manhattanLength() > 3.0;
      update();
      return;
    case Grab::kMove:
    case Grab::kStart:
    case Grab::kEnd:
      DragTo(at, is_free);
      return;
    case Grab::kNone:
      break;
  }
  const std::optional<Bar> bar = BarAt(at);
  const bool is_on = bar.has_value() && at.x() >= kNameWidth;
  const bool is_end = is_on && (at.x() <= bar->box.left() + 6.0 ||
                                at.x() >= bar->box.right() - 6.0);
  setCursor(is_end  ? Qt::SizeHorCursor
            : is_on ? Qt::OpenHandCursor
                    : Qt::ArrowCursor);
}

void CompTimeline::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const bool is_box = grab_ == Grab::kBox;
  if (is_box) {
    FinishBox(event->modifiers());
    return;
  }
  const bool is_clip = grab_ == Grab::kMove || grab_ == Grab::kStart ||
                       grab_ == Grab::kEnd;
  const bool narrows = is_clip && !has_moved_ && narrows_on_release_;
  if (narrows) {
    managers_.comp->PickClips({grabbed_}, PickMode::kReplace, grabbed_);
  }
  if (is_clip) {
    // A click that never dragged leaves nothing to pop.
    if (has_moved_) {
      feedback_.Settle();
    } else {
      feedback_.Cancel();
    }
    EndDrag();
    return;
  }
  grab_ = Grab::kNone;
}

void CompTimeline::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const std::optional<Bar> bar = BarAt(event->position());
  const bool is_on = bar.has_value();
  if (is_on) {
    feedback_.Cancel();
    drag_.reset();
    grab_ = Grab::kNone;
    managers_.comp->PickClips({bar->ref}, PickMode::kAdd, bar->ref);
    FitPicked();
  }
}

void CompTimeline::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(Length() > 0);
  const WheelUse use = scale_.Wheel(*event);
  // The Comp rows do not scroll: Shift's wheel is left to a parent.
  const bool is_rows = use == WheelUse::kRows;
  if (is_rows) {
    event->ignore();
    return;
  }
  update();
  event->accept();
}

}  // namespace snapper
