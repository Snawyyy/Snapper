// PoseTool's side of drags that talk: the words a drag says.

#include <cassert>

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
  assert(kChipGap > 0.0);
  const bool is_talking = drag_ != nullptr && !drag_->readout.text.isEmpty();
  assert(!is_talking || drag_->kind != Kind::kBox);
  return is_talking ? std::optional<Readout>(drag_->readout) : std::nullopt;
}

}  // namespace snapper
