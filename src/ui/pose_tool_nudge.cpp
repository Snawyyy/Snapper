// PoseTool's arrow-key nudge.

#include <cassert>
#include <cmath>
#include <memory>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/pose_tool.h"

namespace snapper {

void PoseTool::Nudge(QPointF delta, const StageFrame& frame) {
  assert(std::isfinite(delta.x()) && std::isfinite(delta.y()));
  assert(frame.scale > 0.0);
  const bool can_nudge =
      drag_ == nullptr && !managers_.selection->picks().empty();
  if (!can_nudge) {
    return;
  }
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kMove;
  drag->frame = frame;
  drag->scope =
      std::make_unique<EditScope>(managers_.history, QObject::tr("Nudge"));
  drag_ = std::move(drag);
  MoveAll(delta * frame.scale);
  // Letting the drag go lands its one step.
  drag_.reset();
}

}  // namespace snapper
