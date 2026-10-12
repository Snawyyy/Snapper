#include <QKeyEvent>

#include <cassert>

#include "ui/stage_view.h"

namespace snapper {

bool StageView::CrispKey(QKeyEvent* event, bool is_down) {
  assert(event != nullptr);
  assert(event->type() == QEvent::KeyPress ||
         event->type() == QEvent::KeyRelease);
  const bool is_crisp_key = event->key() == Qt::Key_Backslash &&
                            event->modifiers() == Qt::NoModifier;
  if (!is_crisp_key) {
    return false;
  }
  if (!event->isAutoRepeat()) {
    ShowCrisp(is_down);
  }
  return true;
}

void StageView::ShowCrisp(bool is_on) {
  const bool is_change = renderer_.cache()->is_crisp_only() != is_on;
  if (!is_change) {
    return;
  }
  renderer_.cache()->set_crisp_only(is_on);
  was_renderer_.cache()->set_crisp_only(is_on);
  update();
  assert(renderer_.cache()->is_crisp_only() == is_on);
}

}  // namespace snapper
