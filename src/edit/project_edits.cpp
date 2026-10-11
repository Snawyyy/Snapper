#include "edit/project_edits.h"

#include <variant>

namespace snapper {

const DollLayer* PosedLayerOf(const Project& project, ShotId shot,
                              LayerId layer) {
  assert(shot.value() >= 0);
  assert(layer.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* item =
      found != nullptr && layer.IsValid() ? FindLayer(*found, layer) : nullptr;
  return item != nullptr ? std::get_if<DollLayer>(&item->content) : nullptr;
}

const Doll* DollOfLayer(const Project& project, ShotId shot,
                        LayerId layer) {
  assert(shot.value() >= 0);
  assert(layer.value() >= 0);
  const DollLayer* posed = PosedLayerOf(project, shot, layer);
  return posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
}

Project WithCopiedLayerDots(Project project,
                            const std::map<LayerId, LayerId>& copies) {
  assert(copies.size() < 1000000);
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  for (auto& [name, shared] : project.dolls) {
    Doll doll = *shared;
    bool is_changed = false;
    for (RigPiece& piece : doll.rig.pieces) {
      for (const auto& [from, to] : copies) {
        const auto own = piece.layer_dots.find(from);
        const bool has_own = own != piece.layer_dots.end();
        if (has_own) {
          piece.layer_dots[to] = own->second;
          is_changed = true;
        }
      }
    }
    if (is_changed) {
      shared = std::make_shared<const Doll>(std::move(doll));
    }
  }
  return project;
}

LayerId TakeLayerId(Project* project) {
  assert(project != nullptr);
  assert(project->next_layer_id >= 1);
  return LayerId(project->next_layer_id++);
}

ShotId TakeShotId(Project* project) {
  assert(project != nullptr);
  assert(project->next_shot_id >= 1);
  return ShotId(project->next_shot_id++);
}

ClipId TakeClipId(Project* project) {
  assert(project != nullptr);
  assert(project->next_clip_id >= 1);
  return ClipId(project->next_clip_id++);
}

}  // namespace snapper
