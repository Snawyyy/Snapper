#ifndef SNAPPER_UI_TIMELINE_KEYS_H_
#define SNAPPER_UI_TIMELINE_KEYS_H_

#include <QKeyEvent>

#include "ui/timeline_scale.h"

namespace snapper {

// The keys the Pose, Video and Comp timelines share. Ctrl+= and Ctrl+-
// zoom round the playhead (playhead_frame), Ctrl+0 shows the whole
// length. True when the key was one of them.
bool ViewKey(const QKeyEvent& event, TimelineScale* scale,
             double playhead_frame);

// Alt+Left and Alt+Right nudge what is picked a frame back or on, Shift
// ten: -1, 1, -10 or 10; 0 for any other key.
int NudgeOf(const QKeyEvent& event);

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_KEYS_H_
