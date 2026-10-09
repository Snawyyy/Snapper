// ShotStrip's ghost: the shot blocks a drag moves or lengthens.

#include <algorithm>
#include <cassert>

#include "edit/selection_manager.h"
#include "ui/shot_strip.h"

namespace snapper {

void ShotStrip::GrabGhost() {
  assert(managers_.selection != nullptr);
  assert(feedback_.parent() == this);
  const bool is_dragging = drag_ != Drag::kNone;
  if (!is_dragging) {
    return;
  }
  // A reorder moves the one shot; an edge drag lengthens every pick.
  const std::vector<ShotId> picked = PickedShots();
  const auto is_dragged = [&](ShotId shot) {
    return drag_ == Drag::kMove
               ? shot == dragged_
               : std::find(picked.begin(), picked.end(), shot) !=
                     picked.end();
  };
  feedback_.SetGhost(MakeGhost(
      GhostKind::kShot, size(), devicePixelRatioF(),
      [&](QPainter* painter) {
        for (const Block& block : Blocks()) {
          const bool is_ghosted = is_dragged(block.shot);
          if (is_ghosted) {
            PaintBlock(painter, block);
          }
        }
      }));
}

}  // namespace snapper
