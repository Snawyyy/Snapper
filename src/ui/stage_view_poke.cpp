#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>

#include <cassert>

#include "ui/stage_view.h"

namespace snapper {

LayerId StageView::LayerUnder(QPointF point, const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const PoseTool::Hover now = tool_.HoverAt(point, frame);
  return now.pick ? now.pick->layer : LayerId();
}

bool StageView::PokeKey(QKeyEvent* event, bool is_down) {
  assert(event != nullptr);
  assert(event->type() == QEvent::KeyPress ||
         event->type() == QEvent::KeyRelease);
  const auto frame = CurrentFrame();
  const bool is_poke_key = event->key() == Qt::Key_P &&
                           event->modifiers() == Qt::NoModifier &&
                           !event->isAutoRepeat() && frame.has_value();
  if (!is_poke_key) {
    return false;
  }
  const QPointF cursor = mapFromGlobal(QCursor::pos());
  if (is_down) {
    poke_.Arm(*frame, LayerUnder(cursor, *frame));
  } else {
    poke_.Disarm();
  }
  const bool is_idle = !tool_.IsDragging();
  if (is_idle) {
    UpdateHover(cursor);
  }
  return true;
}

void StageView::keyReleaseEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_handled = PokeKey(event, false) || CrispKey(event, false);
  if (is_handled) {
    event->accept();
    return;
  }
  QWidget::keyReleaseEvent(event);
  assert(event != nullptr);
}

void StageView::focusOutEvent(QFocusEvent* event) {
  assert(event != nullptr);
  ShowCrisp(false);
  const bool was_armed = poke_.IsArmed();
  if (was_armed) {
    poke_.Disarm();
    unsetCursor();
  }
  QWidget::focusOutEvent(event);
  assert(!poke_.IsArmed());
}

}  // namespace snapper
