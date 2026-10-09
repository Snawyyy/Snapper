#include "anim/doll_beat.h"

#include <cassert>
#include <iterator>
#include <map>
#include <variant>

namespace snapper {
namespace {

// The most keys a channel may hold, for a sanity check.
constexpr size_t kMaxBeatKeys = 20000;

// The doll's keys (its own move's and every piece's), each with whether
// the motion after it eases instead of holding.
std::map<Frame, bool> KeysOf(const Layer& layer) {
  assert(std::holds_alternative<DollLayer>(layer.content));
  std::map<Frame, bool> keys;
  const auto add = [&keys](const Channel<PiecePose>& channel) {
    for (const auto& key : channel.keys) {
      keys[key.frame] = keys[key.frame] || key.ease != Ease::kStep;
    }
  };
  add(layer.transform);
  const auto& pieces = std::get<DollLayer>(layer.content).pieces;
  for (const auto& [name, channel] : pieces) {
    add(channel);
  }
  assert(keys.size() <= layer.transform.keys.size() +
                            pieces.size() * kMaxBeatKeys);
  return keys;
}

}  // namespace

Frame BeatOf(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  const std::map<Frame, bool> keys = KeysOf(layer);
  const auto after = keys.upper_bound(frame);
  const bool has_key_before = after != keys.begin();
  if (!has_key_before) {
    return frame;
  }
  const auto at = std::prev(after);
  const bool is_easing = at->second && after != keys.end();
  if (is_easing) {
    return frame;
  }
  const bool has_earlier = at != keys.begin();
  const int spacing =
      has_earlier ? at->first.index() - std::prev(at)->first.index()
      : after != keys.end() ? after->first.index() - at->first.index()
                            : 1;
  const int since = frame.index() - at->first.index();
  assert(spacing >= 1);
  return Frame(at->first.index() + since / spacing * spacing);
}


}  // namespace snapper
