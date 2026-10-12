#ifndef SNAPPER_EDIT_SHOT_TIMING_H_
#define SNAPPER_EDIT_SHOT_TIMING_H_

#include "model/shot.h"

namespace snapper {

// Changes to a whole shot's timing, used when a shot clip's edge is
// dragged on the reel. Every key, link, hold and layer span moves; keys
// past the shot's end are kept, only hidden.

// Adds delta frames at the shot's start (delta < 0 takes them away):
// everything moves by delta and the shot gets delta frames longer.
// Keys that would land before frame 0 fold onto it, the later one kept,
// so frame 0 shows what the cut frame did. The length stays at least 1.
void ShiftShotFront(Shot* shot, int delta);
// Stretches the shot's timing to length: each frame moves in proportion
// and lands on a whole frame; two keys on one frame keep the later.
void StretchShot(Shot* shot, Frame length);
// How many frames past the shot's end have a key on any channel: keys
// a shorter shot keeps hidden.
int HiddenKeyFrames(const Shot& shot);

}  // namespace snapper

#endif  // SNAPPER_EDIT_SHOT_TIMING_H_
