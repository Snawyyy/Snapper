#include "ui/shot_strip.h"

#include <QColorDialog>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "ui/drag_text.h"
#include "ui/grab.h"
#include "ui/shot_strip_layout.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"

namespace snapper {

ShotStrip::ShotStrip(const Managers& managers)
    : managers_(managers),
      hover_(managers.motion, this),
      feedback_(managers.motion, this) {
  assert(managers_.IsComplete());
  setFixedHeight(kStripHeight);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  connect(&hover_, &HandleHover::Changed, this, [this] { update(); });
  connect(&feedback_, &DragFeedback::Changed, this, [this] { update(); });
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  assert(hasMouseTracking());
}

void ShotStrip::mousePressEvent(QMouseEvent* event) {
  assert(managers_.selection != nullptr);
  assert(event != nullptr);
  const QPointF at = event->position();
  const bool is_left = event->button() == Qt::LeftButton;
  const bool is_add = is_left && AddBlock().contains(at);
  if (is_add) {
    const int after = ShotIndex(managers_.history->current(),
                                managers_.selection->shot());
    const auto added = managers_.shots->Add(after < 0 ? -1 : after + 1);
    if (added) {
      Pick(*added);
    }
    Report(ProblemOf(added));
    return;
  }
  for (const Block& block : Blocks()) {
    const bool is_hit = is_left && block.rect.contains(at);
    if (!is_hit) {
      continue;
    }
    const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool is_ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    const bool is_edge = block.rect.right() - at.x() <= kGrabReach;
    const bool is_picked = managers_.selection->shots().contains(block.shot);
    const bool is_plain = !is_ctrl && !is_shift;
    const bool is_fresh = is_plain && !is_picked;
    if (!is_plain) {
      managers_.selection->PickShots(
          {block.shot}, is_ctrl ? PickMode::kToggle : PickMode::kAdd);
    } else if (is_fresh) {
      Pick(block.shot);
    }
    // Plain-dragging a shot reorders it; dragging any picked shot's edge
    // lengthens every picked shot by the same amount.
    drag_ = is_edge ? Drag::kLength : is_plain ? Drag::kMove : Drag::kNone;
    dragged_ = block.shot;
    drag_start_x_ = at.x();
    drag_frames_ = 0;
    is_started_ = false;
    scope_ = is_edge ? std::make_unique<EditScope>(managers_.history,
                                                    tr("Change shot length"))
                     : nullptr;
    GrabGhost();
    return;
  }
}

void ShotStrip::mouseMoveEvent(QMouseEvent* event) {
  assert(managers_.history != nullptr);
  assert(event != nullptr);
  const QPointF at = event->position();
  UpdateChipHover(at);
  bool is_on_edge = false;
  ShotId edge_shot;
  for (const Block& block : Blocks()) {
    const bool is_here = block.rect.contains(at) &&
                         block.rect.right() - at.x() <= kGrabReach;
    if (is_here) {
      edge_shot = block.shot;
    }
    is_on_edge = is_on_edge || is_here;
  }
  const bool is_free = drag_ == Drag::kNone;
  if (is_free) {
    hover_.Set(is_on_edge ? GrabKind::kEdge : GrabKind::kNone,
               is_on_edge ? QString::number(edge_shot.value()) : QString());
  }
  setCursor(is_on_edge || drag_ == Drag::kLength ? Qt::SizeHorCursor
                                                 : Qt::ArrowCursor);
  // The edge drag waits out the dead zone, then the edge jumps to the
  // cursor.
  is_started_ = is_started_ || IsPastDeadZone(QPointF(drag_start_x_, 0.0),
                                              QPointF(at.x(), 0.0));
  const bool is_resizing = drag_ == Drag::kLength && is_started_;
  if (is_resizing) {
    const int frames = static_cast<int>(
        std::lround((at.x() - drag_start_x_) / kPixelsPerFrame));
    const int step = frames - drag_frames_;
    const bool has_step = step != 0;
    if (has_step) {
      Report(ProblemOf(managers_.shots->ShiftLength(PickedShots(), step)));
      drag_frames_ = frames;
    }
    feedback_.Update(QPointF(drag_start_x_, at.y()), at,
                     FramesText(drag_frames_), drag_frames_ != 0);
  }
  const bool is_reordering = drag_ == Drag::kMove && is_started_;
  if (is_reordering) {
    feedback_.Update(QPointF(drag_start_x_, at.y()), at,
                     PositionText(DropIndex(at.x()) + 1), false);
  }
}

int ShotStrip::DropIndex(double x) const {
  assert(std::isfinite(x));
  int index = 0;
  for (const Block& block : Blocks()) {
    const bool is_past = block.rect.center().x() < x && block.shot != dragged_;
    index += is_past ? 1 : 0;
  }
  assert(index >= 0);
  return index;
}

bool ShotStrip::CancelDrag() {
  assert(feedback_.parent() == this);
  assert(managers_.history != nullptr);
  const bool is_dragging = drag_ != Drag::kNone;
  if (!is_dragging) {
    return false;
  }
  if (scope_) {
    scope_->Cancel();
  }
  scope_.reset();
  drag_ = Drag::kNone;
  feedback_.Cancel();
  update();
  return true;
}

void ShotStrip::UpdateChipHover(QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.history != nullptr);
  ShotId now;
  for (const Block& block : Blocks()) {
    const bool is_here = block.rect.contains(at);
    if (is_here) {
      now = block.shot;
    }
  }
  const bool is_new = now != hovered_;
  hovered_ = now;
  if (is_new) {
    update();
  }
}

void ShotStrip::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(hover_.kind() >= GrabKind::kNone);
  hovered_ = ShotId();
  update();
  hover_.Set(GrabKind::kNone, QString());
  QWidget::leaveEvent(event);
}

void ShotStrip::mouseReleaseEvent(QMouseEvent* event) {
  assert(managers_.history != nullptr);
  assert(event != nullptr);
  const bool is_moving =
      drag_ == Drag::kMove &&
      IsPastDeadZone(QPointF(drag_start_x_, 0.0),
                     QPointF(event->position().x(), 0.0));
  if (is_moving) {
    const int index = DropIndex(event->position().x());
    Report(ProblemOf(managers_.shots->Move(dragged_, index)));
  }
  const bool is_click = drag_ == Drag::kMove && !is_moving;
  if (is_click) {
    // A plain click on a shot already in a bigger pick picks just it.
    Pick(dragged_);
  }
  feedback_.Settle();
  scope_.reset();
  drag_ = Drag::kNone;
}

void ShotStrip::contextMenuEvent(QContextMenuEvent* event) {
  assert(managers_.selection != nullptr);
  assert(event != nullptr);
  for (const Block& block : Blocks()) {
    const bool is_hit = block.rect.contains(event->pos());
    if (is_hit) {
      // Right-clicking outside the pick picks just that shot first.
      const bool is_picked =
          managers_.selection->shots().contains(block.shot);
      if (!is_picked) {
        Pick(block.shot);
      }
      Menu(block.shot, event->globalPos());
      return;
    }
  }
}

void ShotStrip::DeletePicked(const std::vector<ShotId>& picked) {
  assert(managers_.toasts != nullptr);
  assert(!picked.empty());
  managers_.toasts->Finish(
      ProblemOf(managers_.shots->RemoveAll(picked)),
      CountText(tr("Deleted"), static_cast<int>(picked.size()), tr("shot"),
                tr("shots")));
}

void ShotStrip::keyPressEvent(QKeyEvent* event) {
  assert(managers_.history != nullptr);
  assert(event != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool was_dragging = is_escape && CancelDrag();
  if (was_dragging) {
    return;
  }
  const std::vector<ShotId> picked = PickedShots();
  const bool is_delete = !picked.empty() &&
                         (event->key() == Qt::Key_Delete ||
                          event->key() == Qt::Key_Backspace);
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (is_delete) {
    DeletePicked(picked);
    return;
  }
  if (is_all) {
    std::set<ShotId> all;
    for (const auto& shot : managers_.history->current().shots) {
      all.insert(shot->id);
    }
    managers_.selection->PickShots(all, PickMode::kAdd);
    return;
  }
  QWidget::keyPressEvent(event);
}

void ShotStrip::Pick(ShotId shot) {
  assert(managers_.selection != nullptr);
  assert(shot.IsValid());
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, shot);
  managers_.selection->SelectShot(shot);
  const bool is_found = index >= 0;
  if (is_found) {
    managers_.playback->Seek(ShotStart(project, index));
  }
}

std::vector<ShotId> ShotStrip::PickedShots() const {
  assert(managers_.history != nullptr);
  const auto& picked = managers_.selection->shots();
  assert(picked.size() <= static_cast<size_t>(kMaxShots));
  return std::vector<ShotId>(picked.begin(), picked.end());
}

void ShotStrip::Menu(ShotId shot, QPoint where) {
  assert(shot.IsValid());
  const Shot* found = FindShot(managers_.history->current(), shot);
  assert(found != nullptr);
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  connect(menu.addAction(tr("Rename...")), &QAction::triggered, this,
          [this, shot, name = found->name] {
            bool is_ok = false;
            const QString text = QInputDialog::getText(
                this, tr("Rename shot"), tr("Name"), QLineEdit::Normal, name,
                &is_ok);
            if (is_ok) {
              Report(ProblemOf(managers_.shots->Rename(shot, text)));
            }
          });
  connect(menu.addAction(tr("Background colour...")), &QAction::triggered,
          this, [this, shot, colour = found->background] {
            const QColor picked = QColorDialog::getColor(colour, this);
            const bool is_picked = picked.isValid();
            if (is_picked) {
              Report(ProblemOf(
                  managers_.shots->SetBackgroundAll(PickedShots(), picked)));
            }
          });
  connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this,
          [this] {
            Report(ProblemOf(managers_.shots->DuplicateAll(PickedShots())));
          });
  connect(menu.addAction(tr("Delete")), &QAction::triggered, this,
          [this] { DeletePicked(PickedShots()); });
  menu.exec(where);
}

}  // namespace snapper
