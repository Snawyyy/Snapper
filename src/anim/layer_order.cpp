#include "anim/layer_order.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

#include "anim/sampler.h"
#include "model/layer.h"

namespace snapper {
namespace {

// Where layer number `at` of `count` sorts to: twice its place, with a
// layer that moved forward just after the place it reached and one that
// moved back just before it, so it lands past the layer there instead
// of tying with it.
int SortKey(int at, int count, int order) {
  assert(count > 0);
  assert(at >= 0 && at < count);
  const bool is_moved = order != 0;
  if (!is_moved) {
    return 2 * at;
  }
  const int place = std::clamp(at + order, 0, count - 1);
  return 2 * place + (order > 0 ? 1 : -1);
}

}  // namespace

std::vector<const Layer*> LayersInDrawOrder(const Shot& shot, Frame local) {
  assert(local.index() >= 0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  std::vector<const Layer*> live;
  std::vector<int> orders;
  bool has_moved = false;
  for (const Layer& layer : shot.layers) {
    const bool is_live = IsLayerLive(layer, local, shot.length);
    if (!is_live) {
      continue;
    }
    const int order = Sample(layer.transform, local, LayerPose()).order;
    has_moved = has_moved || order != 0;
    live.push_back(&layer);
    orders.push_back(order);
  }
  if (!has_moved) {
    return live;
  }
  const int count = static_cast<int>(live.size());
  std::vector<std::pair<int, const Layer*>> keyed;
  for (int at = 0; at < count; ++at) {
    keyed.emplace_back(SortKey(at, count, orders[static_cast<size_t>(at)]),
                       live[static_cast<size_t>(at)]);
  }
  std::stable_sort(keyed.begin(), keyed.end(),
                   [](const auto& a, const auto& b) {
                     return a.first < b.first;
                   });
  std::vector<const Layer*> out;
  for (const auto& entry : keyed) {
    out.push_back(entry.second);
  }
  return out;
}

}  // namespace snapper
