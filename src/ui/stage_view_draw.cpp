// StageView's way into draw mode: the canvas that covers it and the keys.

#include <cassert>

#include "ui/draw_actions.h"
#include "ui/stage_view.h"

namespace snapper {

void StageView::SetUpDraw() {
  assert(managers_.history != nullptr);
  assert(draw_ == nullptr);
  // The Pose stage shows each shot's draft over it.
  renderer_.SetDraftShown(managers_.draft != nullptr);
  if (managers_.draw == nullptr) {
    return;
  }
  draw_ = std::make_unique<DrawCanvas>(managers_, this);
  AddDrawKeys(this, managers_);
}

}  // namespace snapper
