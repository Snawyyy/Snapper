#ifndef SNAPPER_ANIM_OBJECT_PLAY_H_
#define SNAPPER_ANIM_OBJECT_PLAY_H_

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// The milliseconds a GIF frame of ms shows for: browsers treat 0 to 10
// as 100, and so does Snapper, so a picture plays as it does online.
int FrameMs(int ms);

// The drawing an animated object shows at shot frame, played by play
// over clip's frames on the shot clock, so scrubbing and export show
// the same frame. keyed is the drawing its keys show, kept in Hold mode,
// for a still picture, and for a keyed drawing past the picture's frames
// (one drawn in Snapper).
int PlayedDrawing(const ObjectClip& clip, const ObjectPlay& play,
                  Frame frame, int keyed);

// Puts the played drawing into poses for an object doll; a doll that is
// not an object is left alone.
void ApplyObjectPlay(const Doll& doll, const DollLayer& layer, Frame frame,
                     PoseMap* poses);

}  // namespace snapper

#endif  // SNAPPER_ANIM_OBJECT_PLAY_H_
