// PoseTool's choice of frames: where an edit lands.

#include <cassert>
#include <set>
#include <vector>

#include "edit/selection_manager.h"
#include "ui/pose_tool.h"

namespace snapper {

std::vector<Frame> PoseTool::EditFrames(const StageFrame& at) const {
  assert(managers_.selection != nullptr);
  assert(at.local.index() >= 0);
  const bool is_shown_shot = at.shot == managers_.selection->shot();
  if (!is_shown_shot) {
    return {at.local};
  }
  const std::set<Frame> frames = managers_.selection->EditFrames(at.local);
  return std::vector<Frame>(frames.begin(), frames.end());
}

}  // namespace snapper
