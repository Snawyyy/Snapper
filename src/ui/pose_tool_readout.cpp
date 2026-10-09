// PoseTool's side of drags that talk: the words a drag says, and the
// turn handles' Shift snap.

#include <cassert>
#include <cmath>

#include "edit/pose_manager.h"
#include "ui/drag_text.h"
#include "ui/pose_tool.h"

namespace snapper {

void PoseTool::Say(const QString& text, bool is_snapped, QPointF point) {
  assert(drag_ != nullptr);
  assert(!text.isEmpty());
  drag_->readout.text = text;
  drag_->readout.is_snapped = is_snapped;
  drag_->readout.start = drag_->start;
  drag_->readout.cursor = point;
}

std::optional<PoseTool::Readout> PoseTool::ReadoutOf() const {
  assert(kChipOffset > 0.0);
  const bool is_talking = drag_ != nullptr && !drag_->readout.text.isEmpty();
  assert(!is_talking || drag_->kind != Kind::kBox);
  return is_talking ? std::optional<Readout>(drag_->readout) : std::nullopt;
}

void PoseTool::MoveTurn(QPointF total, QPointF point, bool is_swivel,
                        bool is_shift) {
  assert(drag_ != nullptr);
  assert(std::isfinite(total.x()) && std::isfinite(total.y()));
  const double raw = (is_swivel ? total.x() : total.y()) * kTurnPerPixel;
  const double degrees =
      is_shift ? SnapDegrees(raw, kTurnSnapDegrees) : raw;
  Note(managers_.pose->Lean(drag_->frame.shot, drag_->dolls,
                            drag_->frame.local, is_swivel ? 0.0 : degrees,
                            is_swivel ? degrees : 0.0));
  Say(AngleText(degrees), is_shift && DidSnap(raw, degrees), point);
}

}  // namespace snapper
