#ifndef SNAPPER_UI_DRAG_TEXT_H_
#define SNAPPER_UI_DRAG_TEXT_H_

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace snapper {

// What the value chip says while a handle, key or clip is dragged, and
// where it sits. Pure: no widgets, so tests can check every word.
//
// A turn handle with Shift held snaps to this many degrees.
inline constexpr double kTurnSnapDegrees = 15.0;
// The arrow cursor covers a box this wide and tall, down and right of
// its hotspot. The chip keeps kChipGap pixels clear of that box.
inline constexpr double kCursorBox = 32.0;
inline constexpr double kChipGap = 24.0;

// Whole degrees with a unit: "34 deg", "-12 deg".
QString AngleText(double degrees);
// A move in pixels, signed: "+12, -4 px".
QString OffsetText(QPointF pixels);
// A time shift in frames, signed: "+3 f", "-2 f", "0 f".
QString FramesText(int frames);
// Where a dragged item would land in a row, counting from 1: "Place 3".
QString PositionText(int position);
// A size change as a percentage of the start: 1.5 gives "150 %".
QString ScaleText(double factor);

// degrees rounded to the nearest multiple of step.
double SnapDegrees(double degrees, double step);
// True when snapping moved the value by a visible amount, so the
// drag should flash. A tiny gap is the same spot, not a snap.
bool DidSnap(double raw, double snapped);

// Where the chip of the given size goes for a cursor hotspot: above
// and to its left, kChipGap clear of the cursor box. Near an edge it
// flips to the other side, then is pulled inside bounds. The drag chip
// and the wheel chip both use it.
QRectF ChipRect(QPointF cursor, QSizeF size, const QRectF& bounds);

}  // namespace snapper

#endif  // SNAPPER_UI_DRAG_TEXT_H_
