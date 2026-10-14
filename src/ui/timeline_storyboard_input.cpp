// The Pose timeline's storyboard rows: mouse, keys and menu (see
// timeline_storyboard.h).

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "edit/storyboard_edits.h"
#include "edit/storyboard_manager.h"
#include "ui/form_helpers.h"
#include "ui/grab.h"
#include "ui/pick_keys.h"
#include "ui/problem.h"
#include "ui/storyboard_actions.h"
#include "ui/timeline_eye.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_storyboard.h"
#include "ui/timeline_view.h"

namespace snapper {

bool TimelineStoryboard::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const bool is_view = watched == view_ && managers_.storyboard != nullptr;
  if (!is_view) {
    return QObject::eventFilter(watched, event);
  }
  switch (event->type()) {
    case QEvent::MouseButtonPress:
      return Press(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseMove:
      return MoveTo(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
      return Release(*static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonDblClick:
      return DoubleClick(*static_cast<QMouseEvent*>(event));
    case QEvent::ContextMenu:
      return Menu(*static_cast<QContextMenuEvent*>(event));
    case QEvent::KeyPress:
      return Key(*static_cast<QKeyEvent*>(event));
    case QEvent::ShortcutOverride: {
      // Delete and Escape reach the rows first while frames are picked
      // or a drag runs.
      const auto* key = static_cast<QKeyEvent*>(event);
      const bool is_ours_key = key->key() == Qt::Key_Delete ||
                               key->key() == Qt::Key_Backspace ||
                               key->key() == Qt::Key_Escape;
      const bool has_picked = !managers_.storyboard
                                   ->Picked(Shot(), managers_.storyboard
                                                        ->Active(Shot()))
                                   .empty();
      const bool is_ours = is_ours_key && (drag_ != nullptr || has_picked);
      if (is_ours) {
        event->accept();
      }
      return is_ours;
    }
    default:
      return QObject::eventFilter(watched, event);
  }
}

bool TimelineStoryboard::Press(const QMouseEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(drag_ == nullptr);
  const LayerId layer = LayerAt(event.position());
  const bool is_row = layer.IsValid();
  if (!is_row) {
    // A press anywhere else on the timeline leaves the frames.
    managers_.storyboard->ClearPick();
    return false;
  }
  // Right click opens the row's menu (a ContextMenu event of its own).
  const bool is_left = event.button() == Qt::LeftButton;
  if (!is_left) {
    return true;
  }
  const ShotId shot = Shot();
  const ::snapper::Shot* found =
      FindShot(managers_.history->current(), shot);
  const Layer* inside = found != nullptr ? FindLayer(*found, layer) : nullptr;
  const bool is_gone = inside == nullptr;
  if (is_gone) {
    return true;
  }
  const double top = Top() + RowAt(event.position()) * kRowHeight;
  const bool is_eye = EyeGrabRect(top).contains(event.position());
  if (is_eye) {
    Report(ProblemOf(managers_.stage->SetVisible(shot, layer,
                                                 !inside->is_visible)));
    return true;
  }
  const bool is_name = event.position().x() < kNameWidth;
  if (is_name) {
    // The inspector shows the layer's settings.
    managers_.selection->PickThings({{layer, QString()}}, PickMode::kReplace,
                                    layer);
    Aim(layer);
    return true;
  }
  const auto cel = CelNear(layer, event.position().x());
  if (!cel) {
    Aim(layer);
    return true;
  }
  const bool is_picked = managers_.storyboard->Picked(shot, layer).contains(
      *cel);
  const auto mode = ClickMode(event.modifiers(), is_picked);
  is_narrowing_ = !mode.has_value();
  if (mode) {
    managers_.storyboard->Pick(shot, layer, {*cel}, *mode);
  }
  const std::set<Frame> picked = managers_.storyboard->Picked(shot, layer);
  is_pressed_ = picked.contains(*cel);
  press_ = event.position();
  press_frame_ = *cel;
  press_layer_ = layer;
  origin_ = picked;
  return true;
}

void TimelineStoryboard::StartDrag(LayerId layer) {
  assert(drag_ == nullptr);
  assert(layer.IsValid());
  drag_ = std::make_unique<EditScope>(
      managers_.history, origin_.size() == 1
                             ? tr("Move storyboard frame")
                             : tr("Move storyboard frames"));
  view_->setCursor(Qt::SizeHorCursor);
}

bool TimelineStoryboard::MoveTo(const QMouseEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(std::isfinite(event.position().x()));
  if (!is_pressed_) {
    return false;
  }
  const bool is_dragging =
      drag_ != nullptr || IsPastDeadZone(press_, event.position());
  if (!is_dragging) {
    return true;
  }
  const bool is_started = drag_ != nullptr;
  if (!is_started) {
    StartDrag(press_layer_);
  }
  const auto frame = view_->FrameAt(
      std::max<double>(event.position().x(), kNameWidth));
  const int offset = frame ? frame->index() - press_frame_.index() : 0;
  // A spot the frames can't land on leaves them where they last could.
  const Result<void> moved =
      managers_.storyboard->MoveCels(Shot(), press_layer_, origin_, offset);
  static_cast<void>(moved);
  view_->update();
  return true;
}

bool TimelineStoryboard::Release(const QMouseEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(view_ != nullptr);
  if (!is_pressed_) {
    return false;
  }
  is_pressed_ = false;
  const bool was_drag = drag_ != nullptr;
  if (was_drag) {
    drag_.reset();
    view_->unsetCursor();
  } else if (is_narrowing_) {
    managers_.storyboard->Pick(Shot(), press_layer_, {press_frame_},
                               *ClickMode(event.modifiers(), false));
  }
  is_narrowing_ = false;
  view_->update();
  return true;
}

void TimelineStoryboard::CancelDrag() {
  assert(drag_ != nullptr);
  assert(press_layer_.IsValid());
  drag_->Cancel();
  drag_.reset();
  is_pressed_ = false;
  view_->unsetCursor();
  managers_.storyboard->Pick(Shot(), press_layer_, origin_,
                             PickMode::kReplace);
}

bool TimelineStoryboard::DoubleClick(const QMouseEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(view_ != nullptr);
  const LayerId layer = LayerAt(event.position());
  const bool is_row = layer.IsValid();
  const bool is_on_frames = event.position().x() >= kNameWidth;
  const bool is_drawable = is_row && is_on_frames;
  if (!is_drawable) {
    return is_row;
  }
  const auto frame = view_->FrameAt(event.position().x());
  const StoryboardLayer* board = managers_.storyboard->Of(Shot(), layer);
  const bool can_draw = frame.has_value() && board != nullptr;
  if (!can_draw) {
    return true;
  }
  Aim(layer);
  Seek(*frame);
  // On a frame's own start it draws over it; elsewhere a new one starts.
  RequestStoryboard(managers_, CelStartingAt(*board, *frame) != nullptr);
  return true;
}

bool TimelineStoryboard::Menu(const QContextMenuEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(view_ != nullptr);
  const LayerId layer = LayerAt(event.pos());
  const bool is_row = layer.IsValid();
  if (!is_row) {
    return false;
  }
  const ShotId shot = Shot();
  const auto frame = view_->FrameAt(std::max<double>(event.pos().x(),
                                                     kNameWidth));
  const auto cel = CelNear(layer, event.pos().x());
  const bool is_new_pick =
      cel && !managers_.storyboard->Picked(shot, layer).contains(*cel);
  if (is_new_pick) {
    managers_.storyboard->Pick(shot, layer, {*cel}, PickMode::kReplace);
  } else if (!cel) {
    Aim(layer);
  }
  QMenu menu(view_);
  menu.setToolTipsVisible(true);
  if (frame) {
    Seek(*frame);
  }
  AddMenuActions(&menu, layer);
  menu.exec(event.globalPos());
  return true;
}

void TimelineStoryboard::AddMenuActions(QMenu* menu, LayerId layer) {
  assert(menu != nullptr);
  assert(layer.IsValid());
  const ShotId shot = Shot();
  for (const bool is_over : {false, true}) {
    QAction* draw = menu->addAction(is_over ? tr("Draw over this frame")
                                            : tr("New frame here"));
    draw->setShortcut(is_over ? QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_N)
                              : QKeySequence(Qt::ALT | Qt::Key_N));
    Explain(draw, WhyNoStoryboardHere(managers_, is_over));
    connect(draw, &QAction::triggered, view_, [this, is_over] {
      RequestStoryboard(managers_, is_over);
    });
  }
  menu->addSeparator();
  QAction* remove = menu->addAction(tr("Delete picked frames"));
  remove->setShortcut(QKeySequence(Qt::Key_Delete));
  Explain(remove, managers_.storyboard->WhyNoDelete(shot));
  connect(remove, &QAction::triggered, view_, [this, shot, layer] {
    Report(ProblemOf(managers_.storyboard->DeleteCels(
        shot, layer, managers_.storyboard->Picked(shot, layer))));
  });
  const ::snapper::Shot* found =
      FindShot(managers_.history->current(), shot);
  const Layer* inside = found != nullptr ? FindLayer(*found, layer) : nullptr;
  const StoryboardLayer* board = managers_.storyboard->Of(shot, layer);
  const bool is_known = inside != nullptr && board != nullptr;
  if (!is_known) {
    return;
  }
  const bool is_shown = inside->is_visible;
  QAction* eye = menu->addAction(is_shown ? tr("Hide layer")
                                          : tr("Show layer"));
  connect(eye, &QAction::triggered, view_, [this, shot, layer, is_shown] {
    Report(ProblemOf(managers_.stage->SetVisible(shot, layer, !is_shown)));
  });
  QAction* exported = menu->addAction(tr("In export"));
  exported->setCheckable(true);
  exported->setChecked(board->is_exported);
  exported->setToolTip(tr("Put the storyboard in the exported video, for "
                          "an animatic."));
  const bool is_exported = board->is_exported;
  connect(exported, &QAction::triggered, view_,
          [this, shot, layer, is_exported] {
            Report(ProblemOf(managers_.storyboard->SetExported(
                shot, layer, !is_exported)));
          });
}

bool TimelineStoryboard::Key(const QKeyEvent& event) {
  assert(managers_.storyboard != nullptr);
  assert(view_ != nullptr);
  const ShotId shot = Shot();
  const bool is_escape = event.key() == Qt::Key_Escape;
  const bool is_dragging = drag_ != nullptr;
  const bool is_cancel = is_escape && is_dragging;
  if (is_cancel) {
    CancelDrag();
    return true;
  }
  const LayerId layer = managers_.storyboard->Active(shot);
  const std::set<Frame> picked = managers_.storyboard->Picked(shot, layer);
  const bool has_picked = !picked.empty();
  if (!has_picked) {
    return false;
  }
  const bool is_delete =
      event.key() == Qt::Key_Delete || event.key() == Qt::Key_Backspace;
  if (is_delete) {
    Report(ProblemOf(managers_.storyboard->DeleteCels(shot, layer, picked)));
    return true;
  }
  if (is_escape) {
    managers_.storyboard->ClearPick();
  }
  return is_escape;
}

}  // namespace snapper
