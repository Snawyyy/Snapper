#ifndef SNAPPER_UI_TIMELINE_RULER_H_
#define SNAPPER_UI_TIMELINE_RULER_H_

#include <QPainter>
#include <QRectF>

#include "ui/timeline_scale.h"

namespace snapper {

// Ruler numbers stay this many pixels apart, and ticks this many.
inline constexpr double kMinLabelGap = 24.0;
inline constexpr double kMinTickGap = 3.0;

// The fewest frames between marks that keeps them min_gap pixels apart
// at pixels_per_frame, counted as animators do: every frame, 2, 3, 6,
// 12, then each whole second (24), 2 s, 4 s, 10 s... Every step of a
// second or more is a whole number of seconds, so a numbered mark always
// falls on a second.
int FrameStepFor(double pixels_per_frame, double min_gap);

// The ruler the Pose, Video and Comp timelines share: a band kRulerHeight
// tall across width, dimmed past the last frame, ticks on each frame the
// zoom leaves room for (taller on even frames, tallest on each whole
// second), and frame numbers as far apart as kMinLabelGap, always on
// every second they have room for. Marks and numbers show only right of
// scale.left(); numbers sit over the frame's middle when
// labels_on_middle (where the Pose timeline's keys are), else on its
// start.
void PaintFrameRuler(QPainter* painter, const TimelineScale& scale,
                     int width, bool labels_on_middle);

// The playhead the three timelines share: a 2 pixel line down area, and
// its frame number in a tag on the ruler. x is where the line goes.
void PaintTimelinePlayhead(QPainter* painter, double x, int frame,
                           const QRectF& area);

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_RULER_H_
