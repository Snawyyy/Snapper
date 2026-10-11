// StageView's drop: a doll card dragged here goes on the stage.

#include <QMimeData>

#include <cassert>

#include "ui/doll_cards.h"
#include "ui/doll_drop.h"
#include "ui/stage_view.h"

namespace snapper {

void StageView::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const bool is_doll = !DollsOfDrop(event->mimeData()).isEmpty();
  if (is_doll) {
    is_drop_target_ = true;
    event->acceptProposedAction();
    update();
  }
}

void StageView::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  const bool is_doll = !DollsOfDrop(event->mimeData()).isEmpty();
  if (is_doll) {
    event->acceptProposedAction();
  }
}

void StageView::dragLeaveEvent(QDragLeaveEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  is_drop_target_ = false;
  update();
}

void StageView::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  is_drop_target_ = false;
  const QStringList dolls = DollsOfDrop(event->mimeData());
  const bool is_doll = !dolls.isEmpty();
  if (is_doll) {
    event->acceptProposedAction();
    emit Problem(PutDollsOnStage(managers_, dolls));
  }
  update();
}

}  // namespace snapper
