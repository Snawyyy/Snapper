#ifndef SNAPPER_EDIT_POSE_EDITS_H_
#define SNAPPER_EDIT_POSE_EDITS_H_

#include <algorithm>
#include <type_traits>
#include <variant>

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

// A doll's pose is keyed as a whole, as a drawing is in traditional
// animation: every piece and the layer's own move get a key at frame,
// so one key on the doll's timeline row is the full pose.
inline Result<Project> KeyWholeDoll(Project project, ShotId shot,
                                    LayerId layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.value() >= 0);
  const Doll* doll = DollOfLayer(project, shot, layer);
  const bool is_doll = doll != nullptr;
  if (!is_doll) {
    return project;
  }
  const Rig rig = doll->rig;
  return WithLayer(std::move(project), shot, layer, [&](Layer* edited) {
    auto& pieces = std::get<DollLayer>(edited->content).pieces;
    bool is_set = KeyInPlace(&edited->transform, frame, PiecePose());
    for (const RigPiece& piece : rig.pieces) {
      is_set = KeyInPlace(&pieces[piece.name], frame, PiecePose()) && is_set;
    }
    return is_set ? Result<void>()
                  : Result<void>(std::unexpected(
                        Error{Tr("That doll is full of keys.")}));
  });
}

// The project with track keyed at frame: the value there now (sampled)
// is handed to change(T*) and the result is keyed. An existing key on
// frame keeps its ease; a fresh one steps. Fails if track doesn't hold
// T values. Keying any part of a doll keys the whole doll.
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
  const bool is_doll_part = keyed.has_value() &&
                            (track.kind == TrackKind::kPiece ||
                             track.kind == TrackKind::kLayer);
  if (!is_doll_part) {
    return keyed;
  }
  return KeyWholeDoll(std::move(*keyed), track.shot, track.layer, frame);
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_POSE_EDITS_H_
