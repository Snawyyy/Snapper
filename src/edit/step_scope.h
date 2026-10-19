#ifndef SNAPPER_EDIT_STEP_SCOPE_H_
#define SNAPPER_EDIT_STEP_SCOPE_H_

#include <set>

#include "model/project.h"

namespace snapper {

// What one undo step changed. Pure, so the rules can be read and tested
// without a history.
struct StepScope final {
  // The shots whose own contents changed.
  std::set<ShotId> shots;
  // Something outside any one shot changed: shots added, removed or
  // moved, the characters, the reel, the song, the canvas, the name.
  bool is_shared = false;

  bool operator==(const StepScope&) const = default;
};

// How after differs from before. Id counters are ignored: they only ever
// grow and mean nothing on their own.
StepScope ScopeOf(const Project& before, const Project& after);

// current with scope's shots taken from source; everything else stays as
// it is in current. This is how a step that touched only some shots is
// undone or redone without disturbing the others.
Project WithScopeFrom(Project current, const Project& source,
                      const StepScope& scope);

}  // namespace snapper

#endif  // SNAPPER_EDIT_STEP_SCOPE_H_
