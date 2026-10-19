// StageView's ghost: the picked layers as they stood when a
// drag took hold of them.

#include <QPainter>

#include <cassert>
#include <set>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/stage_view.h"

namespace snapper {
void StageView::GrabGhost(const StageFrame& frame) {
  assert(tool_.IsDragging());
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  std::set<LayerId> only;
  for (const Pick& pick : managers_.selection->picks()) {
    only.insert(pick.layer);
  }
  feedback_.SetGhost(MakeGhost(
      GhostKind::kStagePick, size(), devicePixelRatioF(),
      [&](QPainter* painter) {
        painter->drawImage(frame.corner,
                           renderer_.RenderLayers(project, *shot, frame.local,
                                                  frame.scale, only));
      }));
}

}  // namespace snapper
