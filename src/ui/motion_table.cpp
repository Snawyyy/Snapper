#include "ui/motion_table.h"

#include <QPointF>

#include <array>
#include <cassert>

namespace snapper {
namespace {

constexpr int kModeCount = 3;
constexpr int kKindCount = 5;

// Rows are modes (Full, Calm, Off); columns are kinds (press, hover,
// pop, move, cap).
constexpr std::array<std::array<int, kKindCount>, kModeCount> kMs{{
    {80, 100, 150, 200, 300},
    {60, 80, 100, 120, 180},
    {0, 0, 0, 0, 0},
}};

}  // namespace

int MotionMs(MotionMode mode, MotionKind kind) {
  const int row = static_cast<int>(mode);
  const int column = static_cast<int>(kind);
  assert(row >= 0 && row < kModeCount);
  assert(column >= 0 && column < kKindCount);
  return kMs[static_cast<size_t>(row)][static_cast<size_t>(column)];
}

Bezier MotionBezier(MotionMode mode, MotionKind kind) {
  assert(static_cast<int>(mode) < kModeCount);
  assert(static_cast<int>(kind) < kKindCount);
  const bool is_pop = kind == MotionKind::kPop;
  const bool is_bouncy = is_pop && mode == MotionMode::kFull;
  return is_bouncy ? kEaseOutBack : kEaseOut;
}

QEasingCurve MotionEasing(MotionMode mode, MotionKind kind) {
  const Bezier bezier = MotionBezier(mode, kind);
  assert(bezier.x1 >= 0.0 && bezier.x2 <= 1.0);
  QEasingCurve curve(QEasingCurve::BezierSpline);
  curve.addCubicBezierSegment(QPointF(bezier.x1, bezier.y1),
                              QPointF(bezier.x2, bezier.y2),
                              QPointF(1.0, 1.0));
  assert(curve.type() == QEasingCurve::BezierSpline);
  return curve;
}

QString MotionModeKey(MotionMode mode) {
  assert(static_cast<int>(mode) < kModeCount);
  assert(static_cast<int>(mode) >= 0);
  switch (mode) {
    case MotionMode::kFull:
      return QStringLiteral("full");
    case MotionMode::kCalm:
      return QStringLiteral("calm");
    case MotionMode::kOff:
      return QStringLiteral("off");
  }
  return QStringLiteral("full");
}

MotionMode MotionModeFromKey(const QString& key, MotionMode fallback) {
  assert(static_cast<int>(fallback) < kModeCount);
  assert(static_cast<int>(fallback) >= 0);
  for (int index = 0; index < kModeCount; ++index) {
    const MotionMode mode = static_cast<MotionMode>(index);
    const bool is_match = MotionModeKey(mode) == key;
    if (is_match) {
      return mode;
    }
  }
  return fallback;
}

}  // namespace snapper
