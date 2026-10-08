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
