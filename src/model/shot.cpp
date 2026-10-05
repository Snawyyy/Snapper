#include "model/shot.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const Layer* FindLayer(const Shot& shot, LayerId id) {
  assert(id.IsValid());
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  const auto found =
      std::find_if(shot.layers.begin(), shot.layers.end(),
                   [id](const Layer& layer) { return layer.id == id; });
  return found == shot.layers.end() ? nullptr : &*found;
}

Layer* FindLayer(Shot* shot, LayerId id) {
  assert(shot != nullptr);
  assert(id.IsValid());
  const Layer* layer = FindLayer(static_cast<const Shot&>(*shot), id);
  return const_cast<Layer*>(layer);
}

Frame UsableTransition(const Shot& shot, const Shot* next) {
  assert(shot.length.index() >= 0);
  assert(next == nullptr || next->length.index() >= 0);
  const bool is_cut =
      next == nullptr || shot.transition.kind == TransitionKind::kCut;
  if (is_cut) {
    return Frame(0);
  }
  // Each shot keeps at least one frame of its own.
  const int room = std::min(shot.length.index(), next->length.index()) - 1;
  return Frame(std::min(shot.transition.length.index(), std::max(room, 0)));
}

}  // namespace snapper
