#ifndef SNAPPER_BASE_DIAL_ANGLE_H_
#define SNAPPER_BASE_DIAL_ANGLE_H_

#include <QPointF>

namespace snapper {

// Degrees a snapping dial steps by.
inline constexpr double kDialSnap = 15.0;

// The angle a dial's needle shows for a pointer at offset from the
// dial's centre: 0 points up and turning is clockwise (screen y runs
// down). The answer is the one nearest current among all the angles
// that look the same (360 apart), so turning on past a half turn keeps
// counting. With is_snapping it lands on a multiple of kDialSnap. A
// pointer on the centre leaves current as it is.
double DialAngle(QPointF offset, double current, bool is_snapping);

}  // namespace snapper

#endif  // SNAPPER_BASE_DIAL_ANGLE_H_
