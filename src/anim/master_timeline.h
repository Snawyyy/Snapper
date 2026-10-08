#ifndef SNAPPER_ANIM_MASTER_TIMELINE_H_
#define SNAPPER_ANIM_MASTER_TIMELINE_H_

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where the master track is at one frame. The shots play one after
// another with plain cuts; transitions live on the reel.
struct ShotMoment final {
  // -1 when the project has no shots or the frame is past the end.
  int shot = -1;
  Frame local;
};

// First master frame of the shot at index: where the one before ends.
Frame ShotStart(const Project& project, int index);
Frame TotalLength(const Project& project);
ShotMoment Locate(const Project& project, Frame master);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MASTER_TIMELINE_H_
