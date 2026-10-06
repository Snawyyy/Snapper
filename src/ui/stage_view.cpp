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
#include "edit/selection_manager.h"
#include "edit/project_edits.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/playhead.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Room left around the frame so handles at its edge stay grabbable.
constexpr double kMargin = 0.94;
constexpr double kHandleRadius = 4.0;
constexpr double kLeanWidth = 6.0;
constexpr double kLeanHeight = 14.0;
constexpr int kWheelUnit = 120;

}  // namespace

StageView::StageView(const Managers& managers)
    : managers_(managers), tool_(managers, renderer_.cache()) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(320, 180);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
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
  const double scale =
      kMargin * std::min(width() / double(project.canvas.width),
                         height() / double(project.canvas.height));
  const QPointF corner((width() - project.canvas.width * scale) / 2.0,
                       (height() - project.canvas.height * scale) / 2.0);
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
  const QImage image = renderer_.RenderFrame(
      managers_.history->current(), managers_.playback->frame(),
      frame->scale);
  painter.drawImage(frame->corner, image);
  painter.setRenderHint(QPainter::Antialiasing);
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
}

void StageView::PaintHandles(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const SelectionManager& selection = *managers_.selection;
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_picked = shot != nullptr && selection.shot() == frame.shot &&
                         !selection.picks().empty();
  if (!is_picked) {
    return;
  }
  painter->translate(frame.corner);
  for (const Pick& pick : selection.picks()) {
    const bool is_whole = pick.piece.isEmpty();
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->setBrush(Qt::NoBrush);
    if (is_whole) {
      painter->drawPolygon(LayerShape(project, *shot, pick.layer, frame.local,
                                      frame.scale, renderer_.cache()));
      const auto lean = LeanHandle(project, *shot, pick.layer, frame.local,
                                   frame.scale, renderer_.cache());
      if (lean) {
        // Tall and thin: it drags up and down.
        painter->setPen(QPen(theme::kShadow, 1.0));
        painter->setBrush(theme::kHandle);
        painter->drawRect(QRectF(*lean - QPointF(kLeanWidth / 2,
                                                 kLeanHeight / 2),
                                 QSizeF(kLeanWidth, kLeanHeight)));
      }
      continue;
    }
    const auto outline = PieceOnScreen(project, *shot, pick.layer,
                                       pick.piece, frame.local, frame.scale);
    if (outline) {
      painter->drawPolygon(outline->outline);
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->setBrush(theme::kHandle);
      painter->drawEllipse(outline->pivot, kHandleRadius, kHandleRadius);
    }
  }
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  for (const IkHandle& handle : IkHandles(project, *shot, selection.layer(),
                                          frame.local, frame.scale)) {
    painter->drawRect(QRectF(handle.point - QPointF(kHandleRadius,
                                                    kHandleRadius),
                             QSizeF(kHandleRadius * 2, kHandleRadius * 2)));
  }
}

void StageView::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() &&
                         event->button() == Qt::LeftButton;
  if (is_usable) {
    tool_.Press(event->position(),
                event->modifiers().testFlag(Qt::ShiftModifier),
                event->modifiers().testFlag(Qt::ControlModifier), *frame);
    ReportTool();
  }
  assert(!tool_.IsDragging() || is_usable);
}

void StageView::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_dragging = tool_.IsDragging();
  if (is_dragging) {
    tool_.Move(event->position(),
               event->modifiers().testFlag(Qt::ShiftModifier));
    ReportTool();
    update();
  }
  assert(event->type() == QEvent::MouseMove);
}

void StageView::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (is_left) {
    tool_.Release();
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
    tool_.Wheel(notches, event->modifiers().testFlag(Qt::ShiftModifier),
                *frame);
    ReportTool();
  }
  event->accept();
  assert(event->isAccepted());
}

void StageView::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (is_escape) {
    const bool was_dragging = tool_.IsDragging();
    tool_.Cancel();
    if (!was_dragging) {
      managers_.selection->Clear();
    }
    event->accept();
    return;
  }
  if (is_all) {
    PickAll();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
  assert(!is_escape);
}

void StageView::PickAll() {
  const auto frame = CurrentFrame();
  assert(managers_.selection != nullptr);
  const Shot* shot =
      frame ? FindShot(managers_.history->current(), frame->shot) : nullptr;
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  // Inside a picked doll: all its pieces; otherwise every layer.
  const LayerId focus = managers_.selection->layer();
  const Doll* doll = focus.IsValid()
                         ? DollOfLayer(managers_.history->current(),
                                       frame->shot, focus)
                         : nullptr;
  std::set<Pick> all;
  const bool is_in_doll = doll != nullptr;
  if (is_in_doll) {
    for (const RigPiece& piece : doll->rig.pieces) {
      all.insert({focus, piece.name});
    }
  } else {
    for (const Layer& layer : shot->layers) {
      const bool is_pickable =
          !std::holds_alternative<EffectLayer>(layer.content);
      if (is_pickable) {
        all.insert({layer.id, QString()});
      }
    }
  }
  managers_.selection->SelectShot(frame->shot);
  managers_.selection->PickThings(all, PickMode::kReplace, focus);
}

void StageView::ReportTool() {
  assert(managers_.history != nullptr);
  assert(tool_.problem().size() < 100000);
  emit Problem(tool_.problem());
}

}  // namespace snapper
