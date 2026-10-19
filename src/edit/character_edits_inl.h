#ifndef SNAPPER_EDIT_CHARACTER_EDITS_INL_H_
#define SNAPPER_EDIT_CHARACTER_EDITS_INL_H_

#include <cassert>
#include <memory>

#include "base/text.h"

namespace snapper {

template <typename Change>
Result<Project> WithCharacter(const Project& project, const QString& name,
                              Change change) {
  assert(!name.isEmpty());
  assert(project.characters.size() <=
         static_cast<size_t>(kMaxProjectCharacters));
  const Character* found = FindCharacter(project, name);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(name)});
  }
  Character changed = *found;
  const auto done = change(&changed);
  if (!done) {
    return std::unexpected(done.error());
  }
  assert(IsSound(changed));
  Project next = project;
  next.characters[name] = std::make_shared<const Character>(std::move(changed));
  return next;
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_EDITS_INL_H_
