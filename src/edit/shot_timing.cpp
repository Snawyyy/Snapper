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
    const bool is_same_frame =
        !moved.empty() && moved.back().frame == key.frame;
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
  assert(shot.length.index() >= 0);
  visit(shot.camera);
  for (const Layer& layer : shot.layers) {
    visit(layer.transform);
    const auto* doll = std::get_if<DollLayer>(&layer.content);
    const bool is_doll = doll != nullptr;
    if (is_doll) {
      for (const auto& [name, channel] : doll->pieces) {
        visit(channel);
      }
      for (const auto& [name, channel] : doll->dials) {
        visit(channel);
      }
      visit(doll->skins);
    }
    const auto* effect = std::get_if<EffectLayer>(&layer.content);
    const bool is_effect = effect != nullptr;
    if (is_effect) {
      visit(effect->amount);
    }
  }
}

void RemapLayer(Layer* layer, const FrameMap& map) {
  assert(layer != nullptr);
  assert(map != nullptr);
  RemapChannel(&layer->transform, map);
  auto* doll = std::get_if<DollLayer>(&layer->content);
  const bool is_doll = doll != nullptr;
  if (is_doll) {
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
  const bool is_effect = effect != nullptr;
  if (is_effect) {
    RemapChannel(&effect->amount, map);
  }
  // A layer shown for part of the shot keeps showing the same stretch;
  // length 0 (to the end) stays as it is. One shown from the first
  // frame still is: frames added at the front hold its first pose
  // instead of showing nothing.
  const bool has_span = layer->length.index() > 0;
  const Frame end(layer->start.index() + layer->length.index());
  const bool is_from_first = layer->start.index() == 0;
  layer->start = is_from_first ? Frame(0) : Mapped(map, layer->start);
  if (has_span) {
    layer->length =
        Frame(std::max(1, Mapped(map, end).index() - layer->start.index()));
  }
}

// Moves a Comp effect's keys and span by map, like a layer's.
void RemapEffect(CompEffect* effect, const FrameMap& map) {
  assert(effect != nullptr);
  assert(map != nullptr);
  for (Channel<double>& knob : effect->knobs) {
    RemapChannel(&knob, map);
  }
  const bool is_whole = effect->length == kWholeShot;
  if (is_whole) {
    return;
  }
  // One running from the first frame (or before it) still does.
  const bool is_from_first = effect->start <= 0;
  const int end = effect->start + effect->length;
  effect->start = is_from_first ? effect->start : map(effect->start);
  effect->length = std::max(1, map(end) - effect->start);
}

// Moves every timed thing in shot by map: keys, layer spans, links,
// holds, references and Comp effects. Two holds of one object on one
// frame keep the later.
void RemapShot(Shot* shot, const FrameMap& map) {
  assert(shot != nullptr);
  assert(map != nullptr);
  RemapChannel(&shot->camera, map);
  RemapChannel(&shot->light, map);
  for (Reference& reference : shot->references) {
    reference.start = Mapped(map, reference.start);
  }
  for (CompEffect& effect : shot->comp) {
    RemapEffect(&effect, map);
  }
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
