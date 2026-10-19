// StageView's picking by key: Escape backs out, Ctrl+A picks all.

#include <cassert>
#include <set>
#include <variant>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/stage_view.h"

namespace snapper {

void StageView::BackOut() {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  // Escape backs out one thing at a time: the drag, then the pick.
  const bool was_dragging = tool_.IsDragging();
  tool_.Cancel();
  feedback_.Cancel();
  const bool is_clearing = !was_dragging;
  if (is_clearing) {
    managers_.selection->Clear();
  }
  update();
}

void StageView::PickAll() {
  const auto frame = CurrentFrame();
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  const Shot* shot =
      frame ? FindShot(managers_.history->current(), frame->shot) : nullptr;
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const LayerId focus = managers_.selection->layer();
  std::set<Pick> all;
  for (const Layer& layer : shot->layers) {
    const bool is_pickable = IsStagePicture(layer);
    if (is_pickable) {
      all.insert(Pick{layer.id});
    }
  }
  managers_.selection->SelectShot(frame->shot);
  managers_.selection->PickThings(all, PickMode::kReplace, focus);
}

}  // namespace snapper
