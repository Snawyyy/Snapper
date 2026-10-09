// TimelineView's key drag: sliding picked keys, and what it says.

#include <cassert>

#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/selection_manager.h"
#include "edit/timeline_rows.h"
#include "ui/drag_text.h"
#include "ui/grab.h"
#include "ui/timeline_view.h"

namespace snapper {

void TimelineView::StartKeyDrag(Frame key, QPointF at) {
  assert(key.index() >= 0);
  assert(key_drag_ == nullptr);
  key_drag_ = std::make_unique<EditScope>(managers_.history, tr("Move keys"));
  drag_frame_ = key;
  drag_origin_ = key;
  drag_keys_ = managers_.selection->keys();
  press_ = at;
  is_started_ = false;
  GrabGhost();
}

void TimelineView::GrabGhost() {
  assert(key_drag_ != nullptr);
  assert(managers_.history != nullptr);
  const auto rows =
      TimelineRows(managers_.history->current(), managers_.selection->shot());
  const bool has_rows = !rows.empty();
  if (!has_rows) {
    return;
  }
  feedback_.SetGhost(MakeGhost(
      GhostKind::kKey, size(), devicePixelRatioF(),
      [&](QPainter* painter) { PaintRows(painter, rows, true); }));
}

void TimelineView::SlideKeys(Frame frame, QPointF at) {
  assert(key_drag_ != nullptr);
  assert(frame.index() >= 0);
  is_started_ = is_started_ || IsPastDeadZone(press_, at);
  if (!is_started_) {
    return;
  }
  const int step = frame.index() - drag_frame_.index();
  const bool is_sliding = step != 0;
  if (is_sliding) {
    const auto moved =
        managers_.keys->Shift(managers_.selection->keys(), step);
    if (moved) {
      drag_frame_ = frame;
      managers_.selection->SelectKeys(*moved, false);
      // The playhead shows the frame the key would drop on.
      Seek(drag_frame_);
    } else {
      emit Problem(moved.error().message);
    }
  }
  // Keys always land on whole frames, so each frame crossed flashes.
  const int total = drag_frame_.index() - drag_origin_.index();
  feedback_.Update(press_, at, FramesText(total), total != 0);
}

void TimelineView::EndKeyDrag() {
  assert(managers_.history != nullptr);
  feedback_.Settle();
  is_landing_drag_ = true;
  key_drag_.reset();
  is_landing_drag_ = false;
  is_started_ = false;
  drag_playhead_.End();
  assert(key_drag_ == nullptr);
}

bool TimelineView::CancelKeyDrag() {
  assert(managers_.selection != nullptr);
  const bool is_dragging = key_drag_ != nullptr;
  if (!is_dragging) {
    return false;
  }
  assert(managers_.history != nullptr);
  key_drag_->Cancel();
  key_drag_.reset();
  is_started_ = false;
  feedback_.Cancel();
  managers_.selection->SelectKeys(drag_keys_, false);
  drag_playhead_.Restore();
  update();
  return true;
}

}  // namespace snapper
