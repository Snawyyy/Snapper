#ifndef SNAPPER_EDIT_CHARACTER_MANAGER_INL_H_
#define SNAPPER_EDIT_CHARACTER_MANAGER_INL_H_

// CharacterManager's shared edit plumbing, for its .cpp files only.

#include <cassert>

#include "base/text.h"
#include "edit/character_edits.h"
#include "edit/character_manager.h"
#include "edit/history_manager.h"

namespace snapper {
namespace character_manager_detail {

inline Error NoVector() { return Error{Tr("Pick a vector part to draw on.")}; }

// The drawing of character's vector part, or an error.
inline Result<VectorDrawing*> DrawingOf(Character* character, PartId part) {
  assert(character != nullptr);
  assert(part.value() >= 0);
  CharacterPart* found = FindPart(character, part);
  if (found == nullptr || found->kind != PartKind::kVector) {
    return std::unexpected(NoVector());
  }
  return &found->drawing;
}

// Runs a vector edit on part's drawing with the character's id counter.
template <typename Change>
auto OnDrawing(Character* character, PartId part, Change change)
    -> decltype(change(static_cast<VectorDrawing*>(nullptr),
                       static_cast<int*>(nullptr))) {
  assert(character != nullptr);
  assert(part.value() >= 0);
  const Result<VectorDrawing*> drawing = DrawingOf(character, part);
  if (!drawing) {
    return std::unexpected(drawing.error());
  }
  return change(*drawing, &character->next_id);
}

}  // namespace character_manager_detail

template <typename Change>
Result<void> CharacterManager::Edit(const QString& character,
                                    const QString& label, Change change) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  return history_->Apply(
      label, WithCharacter(history_->current(), character,
                           [&](Character* edited) -> Result<void> {
                             const auto done = change(edited);
                             if (!done) {
                               return std::unexpected(done.error());
                             }
                             return {};
                           }));
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_MANAGER_INL_H_
