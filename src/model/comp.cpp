#include "model/comp.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const CompEffect* FindCompEffect(const std::vector<CompEffect>& comp,
                                 CompEffectId id) {
  assert(comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  assert(id.value() >= 0);
  const auto found = std::ranges::find_if(
      comp, [id](const CompEffect& one) { return one.id == id; });
  return found == comp.end() ? nullptr : &*found;
}

CompEffect* FindCompEffect(std::vector<CompEffect>* comp, CompEffectId id) {
  assert(comp != nullptr);
  assert(id.value() >= 0);
  const CompEffect* found =
      FindCompEffect(static_cast<const std::vector<CompEffect>&>(*comp), id);
  return const_cast<CompEffect*>(found);
}

CompEffectId NextCompEffectId(const std::vector<CompEffect>& comp) {
  assert(comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  int highest = 0;
  for (const CompEffect& one : comp) {
    highest = std::max(highest, one.id.value());
  }
  assert(highest >= 0);
  return CompEffectId(highest + 1);
}

int CompEffectIndex(const std::vector<CompEffect>& comp, CompEffectId id) {
  assert(comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  assert(id.value() >= 0);
  for (size_t i = 0; i < comp.size(); ++i) {
    const bool is_hit = comp[i].id == id;
    if (is_hit) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace snapper
