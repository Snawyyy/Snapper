#ifndef SNAPPER_EDIT_RIG_EDITS_H_
#define SNAPPER_EDIT_RIG_EDITS_H_

// RigManager's building blocks, shared by its source files.

#include <QString>

#include <algorithm>
#include <cassert>
#include <memory>
#include <variant>

#include "base/error.h"
#include "base/text.h"
#include "model/project.h"

namespace snapper {

inline Error NoPiece(const QString& doll, const QString& piece) {
  assert(!doll.isEmpty());
  assert(!piece.isEmpty());
  return Error{Tr("%1 has no piece called %2.").arg(doll, piece)};
}

inline Error NoChain(const QString& doll, const QString& chain) {
  assert(!doll.isEmpty());
  assert(!chain.isEmpty());
  return Error{Tr("%1 has no IK chain called %2.").arg(doll, chain)};
}

// The project with doll's rig changed by change, which may refuse.
template <typename Change>
Result<Project> WithRig(const Project& project, const QString& doll,
                        Change change) {
  assert(!doll.isEmpty());
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  const Doll* found = FindDoll(project, doll);
  const bool is_present = found != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  Doll changed = *found;
  const Result<void> done = change(&changed.rig);
  if (!done) {
    return std::unexpected(done.error());
  }
  Project next = project;
  next.dolls[doll] = std::make_shared<const Doll>(std::move(changed));
  return next;
}

// The same, for one piece of the rig.
template <typename Change>
Result<Project> WithPiece(const Project& project, const QString& doll,
                          const QString& piece, Change change) {
  assert(!piece.isEmpty());
  assert(!doll.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    RigPiece* found = FindRig(rig, piece);
    const bool is_present = found != nullptr;
    if (!is_present) {
      return std::unexpected(NoPiece(doll, piece));
    }
    return change(found);
  });
}

// The same, for one IK chain.
template <typename Change>
Result<Project> WithChain(const Project& project, const QString& doll,
                          const QString& chain, Change change) {
  assert(!chain.isEmpty());
  assert(!doll.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    const auto found =
        std::find_if(rig->chains.begin(), rig->chains.end(),
                     [&chain](const IkChain& c) { return c.name == chain; });
    const bool is_present = found != rig->chains.end();
    if (!is_present) {
      return std::unexpected(NoChain(doll, chain));
    }
    return change(&*found);
  });
}

// Drops warp keys of piece on every layer showing doll.
inline Project WithoutWarpKeys(Project project, const QString& doll,
                        const QString& piece) {
  assert(!doll.isEmpty() && !piece.isEmpty());
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  for (auto& shared : project.shots) {
    Shot shot = *shared;
    bool is_changed = false;
    for (Layer& layer : shot.layers) {
      auto* posed = std::get_if<DollLayer>(&layer.content);
      const bool is_match = posed != nullptr && posed->doll == doll &&
                            posed->pieces.contains(piece);
      if (!is_match) {
        continue;
      }
      for (Key<PiecePose>& key : posed->pieces.at(piece).keys) {
        is_changed = is_changed || !key.value.warp.empty();
        key.value.warp.clear();
      }
    }
    if (is_changed) {
      shared = std::make_shared<const Shot>(std::move(shot));
    }
  }
  return project;
}


}  // namespace snapper

#endif  // SNAPPER_EDIT_RIG_EDITS_H_
