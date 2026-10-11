#include "edit/step_scope.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
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

// A doll with its layers' own warp dot settings taken out.
Doll Plain(Doll doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(doll.rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  for (RigPiece& piece : doll.rig.pieces) {
    piece.layer_dots.clear();
  }
  return doll;
}

// The layers whose dots differ between two versions of a doll.
void AddDotLayers(const Doll& before, const Doll& after,
                  std::set<LayerId>* layers) {
  assert(layers != nullptr);
  assert(before.rig.pieces.size() == after.rig.pieces.size());
  for (size_t i = 0; i < before.rig.pieces.size(); ++i) {
    const auto& a = before.rig.pieces[i].layer_dots;
    const auto& b = after.rig.pieces[i].layer_dots;
    for (const auto& [layer, dots] : a) {
      const auto other = b.find(layer);
      const bool is_changed = other == b.end() || !(other->second == dots);
      if (is_changed) {
        layers->insert(layer);
      }
    }
    for (const auto& entry : b) {
      const bool is_new = !a.contains(entry.first);
      if (is_new) {
        layers->insert(entry.first);
      }
    }
  }
}

// Whether the dolls differ in anything but their layers' own dots, and
// the layers whose dots differ.
bool DollsDiffer(const Project& before, const Project& after,
                 std::set<LayerId>* layers) {
  assert(layers != nullptr);
  assert(before.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  bool is_structural = before.dolls.size() != after.dolls.size();
  for (const auto& [name, doll] : before.dolls) {
    const auto other = after.dolls.find(name);
    const bool is_gone = other == after.dolls.end();
    if (is_gone) {
      is_structural = true;
      continue;
    }
    const bool is_same = doll == other->second || *doll == *other->second;
    if (is_same) {
      continue;
    }
    const bool is_same_but_dots = Plain(*doll) == Plain(*other->second);
    if (is_same_but_dots) {
      AddDotLayers(*doll, *other->second, layers);
    } else {
      is_structural = true;
    }
  }
  return is_structural;
}

// The shot that has layer, in either project; invalid when none does.
ShotId ShotOfLayer(const Project& a, const Project& b, LayerId layer) {
  assert(layer.value() >= 0);
  assert(a.shots.size() <= static_cast<size_t>(kMaxShots));
  for (const Project* project : {&a, &b}) {
    for (const auto& shot : project->shots) {
      const bool has_layer = FindLayer(*shot, layer) != nullptr;
      if (has_layer) {
        return shot->id;
      }
    }
  }
  return ShotId();
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
  const bool is_structural =
      DollsDiffer(before, after, &scope.dot_layers);
  scope.is_shared = scope.is_shared || is_structural;
  for (const LayerId layer : scope.dot_layers) {
    const ShotId shot = ShotOfLayer(before, after, layer);
    const bool is_placed = shot.IsValid();
    if (is_placed) {
      scope.shots.insert(shot);
    }
  }
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
  for (auto& [name, shared] : current.dolls) {
    const auto original = source.dolls.find(name);
    const bool is_in_source = original != source.dolls.end();
    if (!is_in_source) {
      continue;
    }
    Doll doll = *shared;
    for (RigPiece& piece : doll.rig.pieces) {
      const RigPiece* from = FindRig(original->second->rig, piece.name);
      const bool has_source_piece = from != nullptr;
      for (const LayerId layer : scope.dot_layers) {
        const PieceDots* own = nullptr;
        if (has_source_piece) {
          const auto found = from->layer_dots.find(layer);
          const bool is_found = found != from->layer_dots.end();
          own = is_found ? &found->second : nullptr;
        }
        const bool has_own = own != nullptr;
        if (has_own) {
          piece.layer_dots[layer] = *own;
        } else {
          piece.layer_dots.erase(layer);
        }
      }
    }
    const bool is_changed = !(doll == *shared);
    if (is_changed) {
      shared = std::make_shared<const Doll>(std::move(doll));
    }
  }
  return current;
}

}  // namespace snapper
