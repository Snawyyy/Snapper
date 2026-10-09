#include "base/scrub_drag.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

namespace snapper {
namespace {

constexpr double kShiftFactor = 10.0;
constexpr double kAltFactor = 0.1;

// value cut to decimals places.
double Rounded(double value, int decimals) {
  assert(decimals >= 0 && decimals <= 9);
  assert(std::isfinite(value));
  const double scale = std::pow(10.0, decimals);
  return std::round(value * scale) / scale;
}

}  // namespace

ScrubDrag::ScrubDrag(double start, const ScrubRange& range)
    : start_(start), range_(range), value_(start) {
  assert(range_.low <= range_.high);
  assert(range_.unit > 0.0);
}

double ScrubDrag::Move(int dx, ScrubKeys keys) {
  assert(std::isfinite(value_));
  assert(range_.decimals >= 0);
  const bool is_past_dead_zone = std::abs(dx) >= kDeadZone;
  if (is_past_dead_zone) {
    is_active_ = true;
  }
  if (!is_active_) {
    return start_;
  }
  const double factor = keys.shift == keys.alt ? 1.0
                        : keys.shift           ? kShiftFactor
                                               : kAltFactor;
  value_ += (dx - last_dx_) * range_.unit * factor;
  value_ = std::clamp(value_, range_.low, range_.high);
  last_dx_ = dx;
  return Rounded(value_, range_.decimals);
}

}  // namespace snapper
