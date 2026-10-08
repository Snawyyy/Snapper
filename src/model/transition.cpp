#include "model/transition.h"

#include <algorithm>
#include <cassert>

namespace snapper {

Frame UsableLength(const Transition& transition, Frame before, Frame after) {
  assert(before.index() >= 0 && after.index() >= 0);
  assert(static_cast<int>(transition.kind) < kTransitionKindCount);
  const bool is_cut = transition.kind == TransitionKind::kCut;
  if (is_cut) {
    return Frame(0);
  }
  // Each clip keeps at least one frame of its own.
  const int room = std::min(before.index(), after.index()) - 1;
  return Frame(std::clamp(transition.length.index(), 0, std::max(room, 0)));
}

}  // namespace snapper
