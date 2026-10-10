#include "anim/poke_layer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/sampler.h"

namespace snapper {
namespace {

// Drops every key from at on and holds the pose at at from there on.
void HoldFrom(Channel<PiecePose>* channel, Frame at) {
  assert(channel != nullptr);
  assert(at.index() >= 0);
  const PiecePose held = Sample(*channel, at, PiecePose());
  auto& keys = channel->keys;
  keys.erase(std::remove_if(keys.begin(), keys.end(),
                            [at](const Key<PiecePose>& key) {
                              return key.frame >= at;
                            }),
             keys.end());
  // Keys were dropped or the channel had room, so this cannot fail.
  [[maybe_unused]] const bool is_set =
      SetKey(channel, Key<PiecePose>{at, held});
  assert(is_set || keys.size() >= static_cast<size_t>(kMaxKeysPerChannel));
}

}  // namespace

QPointF PokeShove(QPointF move) {
  assert(std::isfinite(move.x()) && std::isfinite(move.y()));
  assert(kPokeShare > 0.0 && kPokeReach > 0.0);
  const QPointF shove = move * kPokeShare;
  const double length = std::hypot(shove.x(), shove.y());
  const bool is_far = length > kPokeReach;
  return is_far ? shove * (kPokeReach / length) : shove;
}

Layer PokeLayer(const Layer& layer, Frame at,
                const std::vector<QPointF>& shoves) {
  assert(std::holds_alternative<DollLayer>(layer.content));
  assert(shoves.size() <= static_cast<size_t>(kMaxPokeFrames));
  Layer poke = layer;
  HoldFrom(&poke.transform, at);
  const PiecePose held = poke.transform.keys.back().value;
  for (size_t i = 0; i < shoves.size(); ++i) {
    PiecePose shoved = held;
    shoved.offset += shoves[i];
    const Frame frame(at.index() + 1 + static_cast<int>(i));
    [[maybe_unused]] const bool is_set =
        SetKey(&poke.transform, Key<PiecePose>{frame, shoved});
    assert(is_set);
  }
  auto& doll = std::get<DollLayer>(poke.content);
  for (auto& [name, channel] : doll.pieces) {
    const bool is_keyed = !channel.keys.empty();
    if (is_keyed) {
      HoldFrom(&channel, at);
    }
  }
  return poke;
}

double PoseDistance(const PoseMap& a, const PoseMap& b) {
  assert(a.size() <= 4096 && b.size() <= 4096);
  double far = 0.0;
  for (const auto& [name, pose] : a) {
    const auto other = b.find(name);
    const bool is_paired = other != b.end() &&
                           other->second.warp.size() == pose.warp.size();
    if (!is_paired) {
      continue;
    }
    for (size_t i = 0; i < pose.warp.size(); ++i) {
      const QPointF gap = pose.warp[i] - other->second.warp[i];
      far = std::max(far, std::hypot(gap.x(), gap.y()));
    }
  }
  assert(far >= 0.0);
  return far;
}

}  // namespace snapper
