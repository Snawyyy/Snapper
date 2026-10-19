#ifndef SNAPPER_EDIT_POSE_EDITS_H_
#define SNAPPER_EDIT_POSE_EDITS_H_

#include <algorithm>
#include <type_traits>

#include "anim/sampler.h"
#include "edit/project_edits.h"

namespace snapper {

// Keys channel at frame with the value it already shows there, if it
// has no key on frame yet. The new key takes the ease of the key before
// it, so the motion around it barely changes.
template <typename T>
bool KeyInPlace(Channel<T>* channel, Frame frame, const T& rest) {
  assert(channel != nullptr);
  assert(frame.index() >= 0);
  const bool has_key = KeyIndexAt(*channel, frame) >= 0;
  if (has_key) {
    return true;
  }
  const auto before = std::find_if(
      channel->keys.rbegin(), channel->keys.rend(),
      [frame](const Key<T>& key) { return key.frame < frame; });
  const Ease ease =
      before == channel->keys.rend() ? Ease::kStep : before->ease;
  return SetKey(channel, Key<T>{frame, Sample(*channel, frame, rest), ease});
}

// The project with track keyed at frame: the value there now (sampled)
// is handed to change(T*) and the result is keyed. An existing key on
// frame keeps its ease; a fresh one steps. Fails if track doesn't hold
// T values.
template <typename T, typename Change>
Result<Project> KeyedAt(const Project& project, const TrackRef& track,
                        Frame frame, const T& rest, Change change) {
  assert(frame.index() >= 0);
  assert(track.shot.value() >= 0);
  auto keyed = WithTrack(project, track, [&](auto* channel) -> Result<void> {
    using Held = typename std::remove_pointer_t<
        decltype(channel)>::value_type;
    constexpr bool is_match = std::is_same_v<Held, T>;
    if constexpr (is_match) {
      T value = Sample(*channel, frame, rest);
      const Result<void> changed = change(&value);
      if (!changed) {
        return changed;
      }
      const int at = KeyIndexAt(*channel, frame);
      const bool has_key = at >= 0;
      const Ease ease = has_key ? channel->keys[static_cast<size_t>(at)].ease
                                : Ease::kStep;
      const bool is_set = SetKey(channel, Key<T>{frame, value, ease});
      if (!is_set) {
        return std::unexpected(Error{Tr("That track is full of keys.")});
      }
      return {};
    } else {
      return std::unexpected(
          Error{Tr("That track doesn't take this kind of change.")});
    }
  });
  return keyed;
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_POSE_EDITS_H_
