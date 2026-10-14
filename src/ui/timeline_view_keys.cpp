// TimelineView's keys and wheel: pick all, copy, paste, delete, the view
// keys and the wheel the three timelines share.

#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QWheelEvent>

#include <cassert>
#include <set>

#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/selection_manager.h"
#include "ui/pick_keys.h"
#include "ui/reference_keys.h"
#include "ui/timeline_keys.h"
#include "ui/timeline_view.h"

namespace snapper {

bool TimelineView::ViewKeyPress(const QKeyEvent& event) {
  assert(event.type() == QEvent::KeyPress);
  assert(FrameWidth() > 0.0);
  // Zoom round the playhead's frame, or the view's middle with none here.
  const auto here = PlayheadHere();
  const double middle = scale_.first() + scale_.VisibleFrames() / 2.0;
  const bool was_view =
      ViewKey(event, &scale_, here ? here->index() + 0.5 : middle);
  if (was_view) {
    ShowChips();
    update();
  }
  return was_view;
}

void TimelineView::NudgeKeys(int delta) {
  assert(delta != 0);
  assert(managers_.keys != nullptr);
  const std::set<KeyRef> picked = managers_.selection->keys();
  const bool has_picks = !picked.empty();
  if (!has_picks) {
    Report(tr("Pick a key to nudge it."));
    return;
  }
  // One undo step: the keys slide together and stay picked.
  const auto moved = managers_.keys->Shift(picked, delta);
  if (!moved) {
    Report(moved.error().message);
    return;
  }
  managers_.selection->SelectKeys(*moved, false);
}

void TimelineView::AddShowAll(QMenu* menu) {
  assert(menu != nullptr);
  assert(FrameWidth() > 0.0);
  QAction* show_all = menu->addAction(tr("Show all\tCtrl+0"));
  connect(show_all, &QAction::triggered, this, [this] {
    scale_.FitAll();
    ShowChips();
    update();
  });
}

void TimelineView::ViewMenu(QPoint where) {
  assert(FrameWidth() > 0.0);
  assert(managers_.selection != nullptr);
  QMenu menu(this);
  AddShowAll(&menu);
  menu.exec(where);
}

void TimelineView::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.keys != nullptr);
  const auto& picked = managers_.selection->keys();
  const bool is_delete = event->key() == Qt::Key_Delete ||
                         event->key() == Qt::Key_Backspace;
  const bool is_copy = event->matches(QKeySequence::Copy);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool was_dragging =
      is_escape && (CancelReferenceDrag() || CancelKeyDrag());
  if (was_dragging) {
    return;
  }
  QString reference_problem;
  const bool was_reference_key =
      ReferenceKey(*event, managers_, &reference_problem);
  if (was_reference_key) {
    Report(reference_problem);
    return;
  }
  const bool was_view_key = ViewKeyPress(*event);
  if (was_view_key) {
    return;
  }
  const int nudge = NudgeOf(*event);
  const bool is_nudge = nudge != 0;
  if (is_nudge) {
    NudgeKeys(nudge);
    return;
  }
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (is_all) {
    const Project& project = managers_.history->current();
    std::set<KeyRef> all;
    for (const TimelineRow& row :
         TimelineRows(project, managers_.selection->shot())) {
      for (const Frame key : row.keys) {
        all.merge(RowKeysAt(project, row, key));
      }
    }
    managers_.selection->PickKeys(all, PickMode::kReplace);
    return;
  }
  const bool is_paste = event->matches(QKeySequence::Paste);
  const auto here = PlayheadHere();
  const bool can_paste_here = is_paste && here.has_value();
  if (is_delete) {
    DeletePicked();
  } else if (is_copy) {
    Report(ProblemOf(managers_.keys->Copy(picked)));
  } else if (can_paste_here) {
    Report(ProblemOf(managers_.keys->Paste(managers_.selection->shot(), *here,
                                 managers_.selection->layer())));
  } else {
    QWidget::keyPressEvent(event);
  }
}

void TimelineView::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(FrameWidth() > 0.0);
  const WheelUse use = scale_.Wheel(*event);
  // The Pose rows do not scroll: Shift's wheel is left to a parent.
  const bool is_rows = use == WheelUse::kRows;
  if (is_rows) {
    event->ignore();
    return;
  }
  event->accept();
  ShowChips();
  update();
}

}  // namespace snapper
