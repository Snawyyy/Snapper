#ifndef SNAPPER_ANIM_REEL_TIMELINE_H_
#define SNAPPER_ANIM_REEL_TIMELINE_H_

#include <set>
#include <vector>

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// One clip showing at a reel frame, and the frame of its source shown.
struct ReelPiece final {
  int track = -1;
  const Clip* clip = nullptr;
  Frame source;
};

// How long clip's source is: the whole video file, or the shot as it is
// now; 0 when its shot is gone.
Frame SourceLength(const Project& project, const Clip& clip);
// The frames of clip that have a picture. A shot shortened after it was
// placed leaves the clip's tail empty rather than moving anything.
Frame ShownLength(const Project& project, const Clip& clip);
// What shows at frame, bottom track first, each with its source frame.
std::vector<ReelPiece> ReelAt(const Project& project, Frame frame);
// From the first frame to the end of the last clip.
Frame ReelLength(const Project& project);
// Where track's last clip ends: the first frame free for good after it.
Frame TrackEnd(const Project& project, int track);
// Every frame where a clip starts or ends, for jumping cut to cut.
std::set<Frame> ReelCuts(const Project& project);

}  // namespace snapper

#endif  // SNAPPER_ANIM_REEL_TIMELINE_H_
