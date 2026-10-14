// Skins win over earlier hand-picked drawings (SkinManager::Wear).

#include "edit/skin_picks.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <variant>

#include "anim/sampler.h"
#include "anim/skin.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// The pieces the skins doll wears in worn name.
std::set<QString> NamedPieces(const Doll& doll, const SkinSet& worn) {
  assert(doll.rig.skins.size() <= static_cast<size_t>(kMaxSkins));
  assert(worn.names.size() <= static_cast<size_t>(kMaxSkins));
  std::set<QString> named;
  for (const Skin& skin : doll.rig.skins) {
    const bool is_worn =
        std::ranges::find(worn.names, skin.name) != worn.names.end();
    if (!is_worn) {
      continue;
    }
    for (const auto& [piece, drawing] : skin.drawings) {
      named.insert(piece);
    }
  }
  return named;
}

// Clears the hand drawing of channel from frame up to end (exclusive),
// keying frame when a pick held from before reaches it.
void ClearPicks(Channel<PiecePose>* channel, Frame frame, Frame end) {
  assert(channel != nullptr);
  assert(frame < end);
  PiecePose held = Sample(*channel, frame, PiecePose());
  const bool is_held_pick = held.drawing >= 0;
  if (is_held_pick) {
    Ease ease = Ease::kStep;
    for (const Key<PiecePose>& key : channel->keys) {
      const bool is_before = !(frame < key.frame);
      ease = is_before ? key.ease : ease;
    }
    held.drawing = -1;
    [[maybe_unused]] const bool is_set =
        SetKey(channel, Key<PiecePose>{frame, held, ease});
  }
  for (Key<PiecePose>& key : channel->keys) {
    const bool is_inside = !(key.frame < frame) && key.frame < end;
    if (is_inside) {
      key.value.drawing = -1;
    }
  }
}

}  // namespace

Result<Project> WithSkinPicksCleared(Project project, ShotId shot,
                                     LayerId layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.value() >= 0);
  const Doll* found = DollOfLayer(project, shot, layer);
  const bool is_doll = found != nullptr;
  if (!is_doll) {
    return project;
  }
  const Doll doll = *found;
  return WithLayer(std::move(project), shot, layer,
                   [&](Layer* edited) -> Result<void> {
                     auto* posed = std::get_if<DollLayer>(&edited->content);
                     const bool is_posed = posed != nullptr;
                     if (!is_posed) {
                       return {};
                     }
                     // Up to where it next wears something else.
                     const SkinSet keyed =
                         Sample(posed->skins, frame, SkinSet());
                     Frame end(kMaxFrame + 1);
                     for (const Key<SkinSet>& key : posed->skins.keys) {
                       const bool is_next = frame < key.frame &&
                                            key.frame < end &&
                                            key.value != keyed;
                       end = is_next ? key.frame : end;
                     }
                     const SkinSet worn = SkinsAt(doll, *posed, frame);
                     for (const QString& piece : NamedPieces(doll, worn)) {
                       const auto channel = posed->pieces.find(piece);
                       const bool has_keys = channel != posed->pieces.end();
                       if (has_keys) {
                         ClearPicks(&channel->second, frame, end);
                       }
                     }
                     return {};
                   });
}

}  // namespace snapper
