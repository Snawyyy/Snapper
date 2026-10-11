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
  std::set<Frame> picked;
  for (const KeyRef& key : managers_.selection->keys()) {
    const bool is_here = key.track.shot == at.shot;
    if (is_here) {
      picked.insert(key.frame);
    }
  }
  const bool has_picked = !picked.empty();
  return has_picked ? std::vector<Frame>(picked.begin(), picked.end())
                    : std::vector<Frame>{at.local};
}

}  // namespace snapper
