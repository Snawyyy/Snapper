#ifndef SNAPPER_ANIM_MASTER_TIMELINE_H_
#define SNAPPER_ANIM_MASTER_TIMELINE_H_

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where the master track is at one frame. During a transition the next
// shot shows too, mixed in by mix (0 = only this shot, 1 = only next).
struct ShotMoment final {
  // -1 when the project has no shots or the frame is past the end.
  int shot = -1;
  Frame local;
  int next_shot = -1;
  Frame next_local;
  double mix = 0.0;
};

// First master frame of the shot at index; each shot starts where the
// one before ends, less its transition overlap.
Frame ShotStart(const Project& project, int index);
Frame TotalLength(const Project& project);
ShotMoment Locate(const Project& project, Frame master);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MASTER_TIMELINE_H_
