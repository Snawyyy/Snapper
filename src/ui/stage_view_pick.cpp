// StageView's picking by key: Escape backs out, Ctrl+A picks all.

#include <cassert>
#include <set>
#include <variant>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/stage_view.h"

namespace snapper {

void StageView::BackOut() {
  assert(managers_.selection != nullptr);
  assert(managers_.history != nullptr);
  // Escape backs out one thing at a time: the drag, the picked warp
  // dot, then the pick.
  const bool was_dragging = tool_.IsDragging();
  tool_.Cancel();
  feedback_.Cancel();
  const bool had_dot = !was_dragging && tool_.DropDot();
  const bool is_clearing = !was_dragging && !had_dot;
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
      const bool is_pickable = IsStagePicture(layer);
      if (is_pickable) {
        all.insert({layer.id, QString()});
      }
    }
  }
  managers_.selection->SelectShot(frame->shot);
  managers_.selection->PickThings(all, PickMode::kReplace, focus);
}

}  // namespace snapper
