#ifndef SNAPPER_UI_MOTION_TABLE_H_
#define SNAPPER_UI_MOTION_TABLE_H_

#include <QEasingCurve>
#include <QString>

namespace snapper {

// How much the interface moves. Full is the design, Calm is shorter and
// never overshoots, Off snaps.
enum class MotionMode { kFull = 0, kCalm = 1, kOff = 2 };

// What is moving. Each kind has one duration and one curve per mode.
enum class MotionKind { kPress, kHover, kPop, kMove, kCap };

// A CSS-style cubic bezier: two control points, ends fixed at (0,0)
// and (1,1).
struct Bezier final {
  double x1;
  double y1;
  double x2;
  double y2;
};

// Hover, press and moves: fast start, soft landing.
inline constexpr Bezier kEaseOut{0.22, 1.0, 0.36, 1.0};
// Pops: about 10% overshoot, then settle.
inline constexpr Bezier kEaseOutBack{0.34, 1.56, 0.64, 1.0};

// Milliseconds for this kind in this mode; 0 in Off.
int MotionMs(MotionMode mode, MotionKind kind);
// The curve for this kind in this mode. Never linear.
Bezier MotionBezier(MotionMode mode, MotionKind kind);
// The same curve as a Qt easing curve, ready for an animation.
QEasingCurve MotionEasing(MotionMode mode, MotionKind kind);

// The name a mode is saved under, and the way back; an unknown name
// gives fallback.
QString MotionModeKey(MotionMode mode);
MotionMode MotionModeFromKey(const QString& key, MotionMode fallback);

}  // namespace snapper

#endif  // SNAPPER_UI_MOTION_TABLE_H_
