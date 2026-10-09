// StageView's ghost: the picked pieces and layers as they stood when a
// drag took hold of them.

#include <QPainter>

#include <cassert>
#include <map>
#include <set>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// The picks as the renderer wants them: a layer picked whole maps to
// nothing (draw all of it), otherwise to the pieces picked.
std::map<LayerId, std::set<QString>> OnlyPicked(const std::set<Pick>& picks) {
  assert(picks.size() < 1000000);
  std::map<LayerId, std::set<QString>> only;
  std::set<LayerId> whole;
  for (const Pick& pick : picks) {
    const bool is_whole = pick.piece.isEmpty();
    if (is_whole) {
      whole.insert(pick.layer);
    }
    only[pick.layer];
    if (!is_whole) {
      only[pick.layer].insert(pick.piece);
    }
  }
  for (const LayerId layer : whole) {
    only[layer].clear();
  }
  assert(only.size() <= picks.size());
  return only;
}

}  // namespace

void StageView::GrabGhost(const StageFrame& frame) {
  assert(tool_.IsDragging());
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const auto only = OnlyPicked(managers_.selection->picks());
  feedback_.SetGhost(MakeGhost(
      GhostKind::kStagePick, size(), devicePixelRatioF(),
      [&](QPainter* painter) {
        painter->drawImage(frame.corner,
                           renderer_.RenderLayers(project, *shot, frame.local,
                                                  frame.scale, only));
      }));
}

}  // namespace snapper
