#ifndef SNAPPER_MODEL_KEY_H_
#define SNAPPER_MODEL_KEY_H_

#include <algorithm>
#include <cassert>
#include <vector>

#include "base/frame.h"

namespace snapper {

// A five-minute song keyed on 1s is 7200 keys; this leaves room.
constexpr int kMaxKeysPerChannel = 20000;

// How a key travels to the next one. Step holds the pose until the next
// key, which is the snappy default.
enum class Ease { kStep, kLinear, kEaseIn, kEaseOut, kEaseInOut };
constexpr int kEaseCount = 5;

template <typename T>
struct Key final {
  Frame frame;
  T value{};
  Ease ease = Ease::kStep;

  bool operator==(const Key&) const = default;
};

// Every animated value in Snapper is a channel: keys sorted by frame,
// at most one per frame. Arms, camera and glitch amount all use this.
template <typename T>
struct Channel final {
  std::vector<Key<T>> keys;

  bool operator==(const Channel&) const = default;
};

// Index of the key on frame, or -1.
template <typename T>
int KeyIndexAt(const Channel<T>& channel, Frame frame) {
  assert(channel.keys.size() <= static_cast<size_t>(kMaxKeysPerChannel));
  assert(frame.index() >= 0);
  const auto found = std::lower_bound(
      channel.keys.begin(), channel.keys.end(), frame,
      [](const Key<T>& key, Frame at) { return key.frame < at; });
  const bool is_hit = found != channel.keys.end() && found->frame == frame;
  return is_hit ? static_cast<int>(found - channel.keys.begin()) : -1;
}

// Adds key, replacing one on the same frame. False when the channel is
// full and key would be a new one.
template <typename T>
bool SetKey(Channel<T>* channel, Key<T> key) {
  assert(channel != nullptr);
  assert(channel->keys.size() <= static_cast<size_t>(kMaxKeysPerChannel));
  auto& keys = channel->keys;
  const auto found = std::lower_bound(
      keys.begin(), keys.end(), key.frame,
      [](const Key<T>& item, Frame at) { return item.frame < at; });
  const bool is_replace = found != keys.end() && found->frame == key.frame;
  if (is_replace) {
    *found = std::move(key);
    return true;
  }
  const bool is_full = keys.size() >= static_cast<size_t>(kMaxKeysPerChannel);
  if (is_full) {
    return false;
  }
  keys.insert(found, std::move(key));
  return true;
}

// False when there was no key on frame.
template <typename T>
bool RemoveKey(Channel<T>* channel, Frame frame) {
  assert(channel != nullptr);
  assert(channel->keys.size() <= static_cast<size_t>(kMaxKeysPerChannel));
  const int index = KeyIndexAt(*channel, frame);
  const bool is_missing = index < 0;
  if (is_missing) {
    return false;
  }
  channel->keys.erase(channel->keys.begin() + index);
  return true;
}

// The invariant every channel keeps: frames strictly rising.
template <typename T>
bool IsSorted(const Channel<T>& channel) {
  assert(channel.keys.size() <= static_cast<size_t>(kMaxKeysPerChannel));
  assert(kEaseCount == 5);
  return std::adjacent_find(channel.keys.begin(), channel.keys.end(),
                            [](const Key<T>& a, const Key<T>& b) {
                              return !(a.frame < b.frame);
                            }) == channel.keys.end();
}

}  // namespace snapper

#endif  // SNAPPER_MODEL_KEY_H_
