#include "edit/project_edits.h"

#include <cassert>

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

ClipId TakeClipId(Project* project) {
  assert(project != nullptr);
  assert(project->next_clip_id >= 1);
  return ClipId(project->next_clip_id++);
}

}  // namespace snapper
