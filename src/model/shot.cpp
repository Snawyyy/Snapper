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

}  // namespace snapper
