#include "ui/dot_shape.h"

#include <QLineF>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

// Offsets under this many pixels are noise, not motion.
constexpr double kStill = 1.5;
// The offset that squishes a blob as far as it goes, and how far that is.
constexpr double kFullSquishAt = 12.0;
constexpr double kMostStretch = 0.5;

}  // namespace

DotShape ShapeOf(std::optional<NodeKind> kind) {
  const bool is_still = !kind.has_value();
  assert(!is_still || !kind.has_value());
  assert(!kind.has_value() || static_cast<int>(*kind) < kNodeKindCount);
  if (is_still) {
    return DotShape::kPlain;
  }
  switch (*kind) {
    case NodeKind::kDrag:
      return DotShape::kRing;
    case NodeKind::kCloth:
      return DotShape::kDiamond;
    case NodeKind::kJelly:
      return DotShape::kBlob;
  }
  return DotShape::kPlain;
}

QPointF TailOf(QPointF shown_from_keyed) {
  const double length = QLineF(QPointF(), shown_from_keyed).length();
  assert(length >= 0.0);
  const bool is_moved = length > kStill;
  if (!is_moved) {
    return QPointF();
  }
  const double tail = std::min(length, kTailMax);
  assert(tail > 0.0 && tail <= kTailMax);
  return -shown_from_keyed * (tail / length);
}

Squish SquishOf(QPointF shown_from_keyed, bool is_animated) {
  const QLineF line(QPointF(), shown_from_keyed);
  assert(line.length() >= 0.0);
  const bool is_moved = is_animated && line.length() > kStill;
  assert(kMostStretch > 0.0);
  if (!is_moved) {
    return Squish();
  }
  const double fraction = std::min(line.length() / kFullSquishAt, 1.0);
  const double along = 1.0 + kMostStretch * fraction;
  return Squish{along, 1.0 / along, -line.angle()};
}

}  // namespace snapper
