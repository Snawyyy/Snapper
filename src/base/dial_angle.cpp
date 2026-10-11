#include "base/dial_angle.h"

#include <QPointF>

#include <cassert>
#include <cmath>
#include <numbers>

namespace snapper {

double DialAngle(QPointF offset, double current, bool is_snapping) {
  assert(std::isfinite(offset.x()) && std::isfinite(offset.y()));
  assert(std::isfinite(current));
  const bool is_centre = std::hypot(offset.x(), offset.y()) < 1e-9;
  if (is_centre) {
    return current;
  }
  const double pointed =
      std::atan2(offset.x(), -offset.y()) * 180.0 / std::numbers::pi;
  const double snapped =
      is_snapping ? std::round(pointed / kDialSnap) * kDialSnap : pointed;
  return current + std::remainder(snapped - current, 360.0);
}

}  // namespace snapper
