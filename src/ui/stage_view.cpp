#include "ui/stage_view.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/poke_manager.h"
#include "edit/selection_manager.h"
#include "edit/project_edits.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/drag_text.h"
#include "ui/playhead.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kWheelUnit = 120;

}  // namespace

StageView::StageView(const Managers& managers)
    : managers_(managers),
      tool_(managers, renderer_.cache()),
      poke_(managers),
      hover_(managers.motion, this),
      feedback_(managers.motion, this),
      watcher_(managers.history, this),
      flash_(managers.motion, this) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::StrongFocus);
  setAcceptDrops(true);
  setMouseTracking(true);
  setMinimumSize(320, 180);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this,
          [this] { tool_.Observe(); });
  connect(managers_.poke, &PokeManager::Changed, this, update_me);
  connect(&hover_, &HandleHover::Changed, this, update_me);
  connect(&feedback_, &DragFeedback::Changed, this, update_me);
  connect(&watcher_, &ChangeWatcher::Stepped, this, &StageView::ShowStep);
  connect(&flash_, &StageFlash::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  assert(focusPolicy() == Qt::StrongFocus);
}

std::optional<StageFrame> StageView::CurrentFrame() const {
  const Project& project = managers_.history->current();
  const auto spot = SpotOf(managers_);
  assert(IsValidCanvas(project.canvas));
  if (!spot) {
    return std::nullopt;
  }
  const QSize canvas(project.canvas.width, project.canvas.height);
  const double scale = FitScale(canvas, size()) * camera_.zoom;
  const QPointF corner = CornerOf(camera_, canvas, size(), scale);
  assert(scale > 0.0);
  return StageFrame{spot->shot, spot->local, scale, corner};
}

void StageView::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  const auto frame = CurrentFrame();
  if (!frame) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No shot here. Add one in the shot strip above."));
    return;
  }
  poke_.Sync(renderer_.cache()->poses());
  const QImage image = renderer_.RenderFrame(
      managers_.history->current(),
      managers_.playback->FrameOn(Timeline::kShots), frame->scale);
  painter.drawImage(frame->corner, image);
  const Shot* shown = FindShot(managers_.history->current(), frame->shot);
  const bool is_empty_shot = shown != nullptr && shown->layers.empty();
  if (is_empty_shot) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Drag a doll here from the Cast, or double-click "
                        "one there."));
  }
  painter.setRenderHint(QPainter::Antialiasing);
  PaintRopes(*frame, &painter);
  PaintHover(*frame, &painter);
  PaintHandles(*frame, &painter);
  const QRectF box = tool_.Box();
  const bool has_box = !box.isEmpty();
  if (has_box) {
    painter.resetTransform();
    painter.setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter.setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                            theme::kPick.blue(), 40));
    painter.drawRect(box);
  }
  painter.resetTransform();
  PaintGuides(&painter);
  if (is_drop_target_) {
    painter.setPen(QPen(theme::kPick, 3.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(frame->corner,
                            QSizeF(image.width(), image.height())));
  }
  flash_.Paint(&painter);
  feedback_.Paint(&painter, QRectF(rect()));
}

void StageView::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_pan = event->button() == Qt::MiddleButton;
  if (is_pan) {
    is_panning_ = true;
    pan_from_ = event->position();
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() &&
                         event->button() == Qt::LeftButton;
  const bool is_poke = is_usable && poke_.Press(
      event->position(), LayerUnder(event->position(), *frame), *frame);
  if (is_poke) {
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  if (is_usable) {
    tool_.Press(event->position(),
                event->modifiers().testFlag(Qt::ShiftModifier),
                event->modifiers().testFlag(Qt::ControlModifier), *frame);
    const bool is_moving = tool_.IsDragging() && !tool_.IsBoxing();
    if (is_moving) {
      GrabGhost(*frame);
    }
    ReportTool();
  }
  assert(!tool_.IsDragging() || is_usable);
}

void StageView::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (is_panning_) {
    camera_ = Panned(camera_, event->position() - pan_from_);
    pan_from_ = event->position();
    update();
    return;
  }
  const auto poke_frame = CurrentFrame();
  const bool is_poking = poke_.IsPoking() && poke_frame.has_value();
  if (is_poking) {
    poke_.Move(event->position(), *poke_frame);
    return;
  }
  const bool is_dragging = tool_.IsDragging();
  if (is_dragging) {
    tool_.Move(event->position(),
               event->modifiers().testFlag(Qt::ShiftModifier));
    ReportTool();
    ShowDrag();
    update();
    return;
  }
  UpdateHover(event->position());
  assert(event->type() == QEvent::MouseMove);
}

void StageView::UpdateHover(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(!tool_.IsDragging());
  const auto frame = CurrentFrame();
  if (!frame) {
    ClearHover();
    return;
  }
  const PoseTool::Hover now = tool_.HoverAt(point, *frame);
  hover_.Set(now.kind, now.key);
  const bool is_pick_changed = now.pick != hover_pick_;
  hover_pick_ = now.pick;
  // A handle shows its own cursor; a thing a click would pick, move.
  const Qt::CursorShape poke_cursor = poke_.CursorOver(
      now.pick ? now.pick->layer : LayerId(), *frame);
  const bool is_poke_cursor = poke_cursor != Qt::ArrowCursor;
  setCursor(is_poke_cursor          ? poke_cursor
            : now.kind != GrabKind::kNone ? CursorFor(now.kind)
            : now.pick.has_value()    ? Qt::SizeAllCursor
                                      : Qt::ArrowCursor);
  if (is_pick_changed) {
    update();
  }
}

void StageView::ClearHover() {
  assert(managers_.motion != nullptr);
  hover_.Set(GrabKind::kNone, QString());
  hover_pick_.reset();
  unsetCursor();
  update();
  assert(!hover_pick_.has_value());
}

void StageView::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const bool is_idle = !tool_.IsDragging();
  if (is_idle) {
    ClearHover();
  }
  QWidget::leaveEvent(event);
}

void StageView::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_panning = is_panning_ && event->button() == Qt::MiddleButton;
  if (was_panning) {
    is_panning_ = false;
    UpdateHover(event->position());
    return;
  }
  const bool is_left = event->button() == Qt::LeftButton;
  const bool was_poking = is_left && poke_.IsPoking();
  if (was_poking) {
    poke_.Release();
    UpdateHover(event->position());
    return;
  }
  if (is_left) {
    tool_.Release();
    feedback_.Settle();
    update();
  }
  assert(!is_left || !tool_.IsDragging());
}

void StageView::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() &&
                         event->button() == Qt::LeftButton;
  if (is_usable) {
    tool_.PickWhole(event->position(), *frame);
    update();
  }
  assert(!tool_.IsDragging());
}

void StageView::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  const bool is_zoom = event->modifiers().testFlag(Qt::ControlModifier);
  if (is_zoom) {
    ZoomBy(event->angleDelta().y() / 120.0, event->position());
    event->accept();
    return;
  }
  // Some systems turn Shift+wheel into a sideways scroll.
  const QPoint turn = event->angleDelta();
  // Touchpads send small steps; they add up to whole notches.
  wheel_left_ += turn.y() != 0 ? turn.y() : turn.x();
  const int notches = wheel_left_ / kWheelUnit;
  wheel_left_ -= notches * kWheelUnit;
  const auto frame = CurrentFrame();
  const bool is_usable = notches != 0 && frame.has_value() &&
                         !tool_.IsDragging();
  if (is_usable) {
    const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool was_dot = tool_.PickedDot(*frame).has_value();
    tool_.Wheel(notches, is_fine, *frame);
    ReportTool();
    const bool has_turned = !was_dot && tool_.problem().isEmpty();
    if (has_turned) {
      ShowWheel(event->position(), notches, is_fine);
    }
  }
  event->accept();
  assert(event->isAccepted());
}

void StageView::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  const bool was_poke_ended = is_escape && poke_.Cancel();
  if (was_poke_ended) {
    update();
    event->accept();
    return;
  }
  const bool was_nudge = NudgeKey(event);
  if (was_nudge) {
    event->accept();
    return;
  }
  const bool was_camera_key = CameraKey(event);
  if (was_camera_key) {
    event->accept();
    return;
  }
  const bool was_poke_key = PokeKey(event, true);
  if (was_poke_key) {
    event->accept();
    return;
  }
  if (is_escape) {
    BackOut();
    event->accept();
    return;
  }
  if (is_all) {
    PickAll();
    event->accept();
    return;
  }
  const auto frame = CurrentFrame();
  const bool is_erase =
      event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace;
  const bool has_cleared =
      is_erase && frame.has_value() && tool_.ClearDots(*frame);
  if (has_cleared) {
    ReportTool();
    event->accept();
    return;
  }
  const bool is_drag_key = event->key() == Qt::Key_D &&
                           event->modifiers() == Qt::NoModifier &&
                           frame.has_value();
  if (is_drag_key) {
    tool_.ToggleDrag(*frame);
    ReportTool();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
  assert(!is_escape);
}

void StageView::ShowDrag() {
  assert(tool_.IsDragging());
  assert(managers_.motion != nullptr);
  const auto readout = tool_.ReadoutOf();
  if (readout) {
    feedback_.Update(readout->start, readout->cursor, readout->text,
                     readout->is_snapped);
  }
}

void StageView::ShowWheel(QPointF cursor, int notches, bool is_fine) {
  assert(notches != 0);
  assert(std::isfinite(cursor.x()) && std::isfinite(cursor.y()));
  // A rested chip has faded: the next turn counts from nothing.
  const bool is_fresh = feedback_.pulse() <= 0.0;
  wheel_degrees_ = is_fresh ? 0.0 : wheel_degrees_;
  wheel_degrees_ += PoseTool::WheelDegrees(notches, is_fine);
  feedback_.Pulse(cursor, AngleText(wheel_degrees_));
}

void StageView::ReportTool() {
  assert(managers_.history != nullptr);
  assert(tool_.problem().size() < 100000);
  emit Problem(tool_.problem());
}

}  // namespace snapper
