#include "edit/step_scope.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace snapper {
namespace {

const Shot* ShotOf(const Project& project, ShotId id) {
  assert(id.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  return FindShot(project, id);
}

bool SameShot(const Shot* a, const Shot* b) {
  assert(a == nullptr || a->layers.size() <= kMaxLayersPerShot);
  assert(b == nullptr || b->layers.size() <= kMaxLayersPerShot);
  return a == b || (a != nullptr && b != nullptr && *a == *b);
}

// The ids of the shots, in order.
std::vector<ShotId> OrderOf(const Project& project) {
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(kMaxShots > 0);
  std::vector<ShotId> order;
  for (const auto& shot : project.shots) {
    order.push_back(shot->id);
  }
  return order;
}

// True when the characters differ: they are shared by every shot.
bool CharactersDiffer(const Project& before, const Project& after) {
  assert(before.characters.size() <=
         static_cast<size_t>(kMaxProjectCharacters));
  assert(after.characters.size() <=
         static_cast<size_t>(kMaxProjectCharacters));
  if (before.characters.size() != after.characters.size()) {
    return true;
  }
  return std::ranges::any_of(before.characters, [&after](const auto& entry) {
    const auto other = after.characters.find(entry.first);
    return other == after.characters.end() ||
           !(entry.second == other->second ||
             *entry.second == *other->second);
  });
}

}  // namespace

StepScope ScopeOf(const Project& before, const Project& after) {
  assert(before.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(after.shots.size() <= static_cast<size_t>(kMaxShots));
  StepScope scope;
  scope.is_shared = OrderOf(before) != OrderOf(after) ||
                    before.name != after.name ||
                    before.canvas != after.canvas ||
                    before.song != after.song || !(before.reel == after.reel);
  for (const auto& shot : before.shots) {
    const bool is_changed =
        !SameShot(shot.get(), ShotOf(after, shot->id));
    if (is_changed) {
      scope.shots.insert(shot->id);
    }
  }
  scope.is_shared = scope.is_shared || CharactersDiffer(before, after);
  return scope;
}

Project WithScopeFrom(Project current, const Project& source,
                      const StepScope& scope) {
  assert(current.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(source.shots.size() <= static_cast<size_t>(kMaxShots));
  for (const ShotId id : scope.shots) {
    const int at = ShotIndex(current, id);
    const int from = ShotIndex(source, id);
    const bool can_take = at >= 0 && from >= 0;
    if (can_take) {
      current.shots[static_cast<size_t>(at)] =
          source.shots[static_cast<size_t>(from)];
    }
  }
  return current;
}

}  // namespace snapper
