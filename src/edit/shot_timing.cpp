#include "edit/shot_timing.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace snapper {
namespace {

// A frame's new place, never off the timeline. It must never fall as
// the frame rises, so keys stay in order.
using FrameMap = std::function<int(int)>;

Frame Mapped(const FrameMap& map, Frame frame) {
  assert(map != nullptr);
  assert(frame.index() >= 0);
  return Frame(std::clamp(map(frame.index()), 0, kMaxFrame));
}

// Moves every key of channel by map; keys landing on one frame keep the
// later one.
template <typename T>
void RemapChannel(Channel<T>* channel, const FrameMap& map) {
  assert(channel != nullptr);
  assert(IsSorted(*channel));
  std::vector<Key<T>> moved;
  moved.reserve(channel->keys.size());
  for (Key<T>& key : channel->keys) {
    key.frame = Mapped(map, key.frame);
    const bool is_same_frame = !moved.empty() && moved.back().frame == key.frame;
    if (is_same_frame) {
      moved.back() = std::move(key);
    } else {
      moved.push_back(std::move(key));
    }
  }
  channel->keys = std::move(moved);
}

// Calls visit(const Channel<T>&) on every channel of keys in shot.
template <typename Visit>
void ReadChannels(const Shot& shot, Visit visit) {
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  visit(shot.camera);
  for (const Layer& layer : shot.layers) {
    visit(layer.transform);
    const auto* doll = std::get_if<DollLayer>(&layer.content);
    if (doll != nullptr) {
      for (const auto& [name, channel] : doll->pieces) {
        visit(channel);
      }
      for (const auto& [name, channel] : doll->dials) {
        visit(channel);
      }
      visit(doll->skins);
    }
    const auto* effect = std::get_if<EffectLayer>(&layer.content);
    if (effect != nullptr) {
      visit(effect->amount);
    }
  }
}

void RemapLayer(Layer* layer, const FrameMap& map) {
  assert(layer != nullptr);
  assert(map != nullptr);
  RemapChannel(&layer->transform, map);
  auto* doll = std::get_if<DollLayer>(&layer->content);
  if (doll != nullptr) {
    for (auto& [name, channel] : doll->pieces) {
      RemapChannel(&channel, map);
    }
    for (auto& [name, channel] : doll->dials) {
      RemapChannel(&channel, map);
    }
    RemapChannel(&doll->skins, map);
    doll->play.start = Mapped(map, doll->play.start);
  }
  auto* effect = std::get_if<EffectLayer>(&layer->content);
  if (effect != nullptr) {
    RemapChannel(&effect->amount, map);
  }
  // A layer shown for part of the shot keeps showing the same stretch;
  // length 0 (to the end) stays as it is.
  const bool has_span = layer->length.index() > 0;
  const Frame end(layer->start.index() + layer->length.index());
  layer->start = Mapped(map, layer->start);
  if (has_span) {
    layer->length =
        Frame(std::max(1, Mapped(map, end).index() - layer->start.index()));
  }
}

// Moves every timed thing in shot by map: keys, layer spans, links and
// holds. Two holds of one object on one frame keep the later.
void RemapShot(Shot* shot, const FrameMap& map) {
  assert(shot != nullptr);
  assert(map != nullptr);
  RemapChannel(&shot->camera, map);
  for (Layer& layer : shot->layers) {
    RemapLayer(&layer, map);
  }
  for (Link& link : shot->links) {
    link.from = Mapped(map, link.from);
  }
  std::map<std::pair<LayerId, Frame>, size_t> last;
  for (size_t i = 0; i < shot->holds.size(); ++i) {
    Hold& hold = shot->holds[i];
    hold.at = Mapped(map, hold.at);
    last[{hold.object, hold.at}] = i;
  }
  std::vector<Hold> kept;
  for (size_t i = 0; i < shot->holds.size(); ++i) {
    const Hold& hold = shot->holds[i];
    const bool is_last = last[{hold.object, hold.at}] == i;
    if (is_last) {
      kept.push_back(hold);
    }
  }
  shot->holds = std::move(kept);
}

}  // namespace

void ShiftShotFront(Shot* shot, int delta) {
  assert(shot != nullptr);
  assert(std::abs(delta) <= kMaxFrame);
  RemapShot(shot, [delta](int frame) { return frame + delta; });
  shot->length =
      Frame(std::clamp(shot->length.index() + delta, 1, kMaxFrame));
}

void StretchShot(Shot* shot, Frame length) {
  assert(shot != nullptr);
  assert(length.index() >= 1 && length.index() <= kMaxFrame);
  const double factor = static_cast<double>(length.index()) /
                        std::max(1, shot->length.index());
  RemapShot(shot, [factor](int frame) {
    return static_cast<int>(std::lround(frame * factor));
  });
  shot->length = length;
}

int HiddenKeyFrames(const Shot& shot) {
  assert(shot.length.index() >= 0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  std::set<Frame> hidden;
  ReadChannels(shot, [&](const auto& channel) {
    for (const auto& key : channel.keys) {
      const bool is_past = !(key.frame < shot.length);
      if (is_past) {
        hidden.insert(key.frame);
      }
    }
  });
  return static_cast<int>(hidden.size());
}

}  // namespace snapper
