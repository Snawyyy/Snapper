#include "edit/project_edits.h"

namespace snapper {

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

}  // namespace snapper
