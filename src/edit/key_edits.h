#ifndef SNAPPER_EDIT_KEY_EDITS_H_
#define SNAPPER_EDIT_KEY_EDITS_H_

#include <algorithm>
#include <cassert>
#include <vector>

#include "model/key.h"

namespace snapper {

// Moves every key at or after frame by delta; with delta < 0, keys in
// the span that closes up are dropped.
template <typename T>
bool SlideFrom(Channel<T>* channel, Frame frame, int delta) {
  assert(channel != nullptr);
  assert(frame.index() >= 0);
  const int gap_end = frame.index() - std::min(delta, 0);
  std::erase_if(channel->keys, [&](const Key<T>& key) {
    return key.frame.index() >= frame.index() &&
           key.frame.index() < gap_end;
  });
  for (Key<T>& key : channel->keys) {
    const bool is_later = !(key.frame < frame);
    if (is_later) {
      const int moved = key.frame.index() + delta;
      const bool is_on_timeline = moved >= 0 && moved <= kMaxFrame;
      if (!is_on_timeline) {
        return false;
      }
      key.frame = Frame(moved);
    }
  }
  return IsSorted(*channel);
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_KEY_EDITS_H_
