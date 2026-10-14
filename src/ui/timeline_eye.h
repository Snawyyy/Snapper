#ifndef SNAPPER_UI_TIMELINE_EYE_H_
#define SNAPPER_UI_TIMELINE_EYE_H_

#include <QPainter>
#include <QRectF>

namespace snapper {

// The eye at the right of a timeline row's name (the reference rows and
// the storyboard rows): its size, and where it sits in the row that
// starts at top.
inline constexpr double kEyeSize = 14.0;
QRectF EyeRect(double top);
// How far past the eye a click still counts as on it.
QRectF EyeGrabRect(double top);

// The eye in box: open with a pupil when is_open, a line when shut.
void PaintEye(const QRectF& box, bool is_open, QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_EYE_H_
