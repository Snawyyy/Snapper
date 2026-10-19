// StageView's way into draw mode: the canvas that covers it.

#include <cassert>

#include "ui/stage_view.h"

namespace snapper {

void StageView::SetUpDraw() {
  assert(managers_.history != nullptr);
  assert(draw_ == nullptr);
  // The Pose stage shows storyboard layers at their place in the stack.
  renderer_.SetStoryboardShown(managers_.storyboard != nullptr);
  if (managers_.draw == nullptr) {
    return;
  }
  draw_ = std::make_unique<DrawCanvas>(managers_, this);
}

}  // namespace snapper
