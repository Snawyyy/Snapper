#include "ui/drag_text.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// Gaps under this are the same spot, not a snap.
constexpr double kSnapEpsilon = 0.01;

// The nearest whole number, as an int (so it never prints "-0").
int Whole(double value) {
  assert(std::isfinite(value));
  assert(std::abs(value) < 1.0e9);
  return static_cast<int>(std::lround(value));
}

QString Signed(int value) {
  assert(value > -1000000000);
  assert(value < 1000000000);
  return value > 0 ? QStringLiteral("+%1").arg(value)
                   : QString::number(value);
}

}  // namespace

QString AngleText(double degrees) {
  assert(std::isfinite(degrees));
  assert(std::abs(degrees) < 1.0e9);
  return QStringLiteral("%1 deg").arg(Whole(degrees));
}

QString OffsetText(QPointF pixels) {
  assert(std::isfinite(pixels.x()));
  assert(std::isfinite(pixels.y()));
  return QStringLiteral("%1, %2 px")
      .arg(Signed(Whole(pixels.x())), Signed(Whole(pixels.y())));
}

QString FramesText(int frames) {
  assert(frames > -1000000000);
  assert(frames < 1000000000);
  return QStringLiteral("%1 f").arg(Signed(frames));
}

QString PositionText(int position) {
  assert(position >= 1);
  assert(position < 1000000000);
  return QStringLiteral("Place %1").arg(position);
}

QString ScaleText(double factor) {
  assert(std::isfinite(factor));
  assert(factor >= 0.0);
  return QStringLiteral("%1 %").arg(Whole(factor * 100.0));
}

double SnapDegrees(double degrees, double step) {
  assert(std::isfinite(degrees));
  assert(step > 0.0);
  return std::round(degrees / step) * step;
}

bool DidSnap(double raw, double snapped) {
  assert(std::isfinite(raw));
  assert(std::isfinite(snapped));
  return std::abs(raw - snapped) > kSnapEpsilon;
}

QRectF ChipRect(QPointF cursor, QSizeF size, const QRectF& bounds) {
  assert(size.width() >= 0.0 && size.height() >= 0.0);
  assert(bounds.isValid());
  // Default: above and left, the side the arrow does not cover.
  double x = cursor.x() - kChipGap - size.width();
  double y = cursor.y() - kChipGap - size.height();
  const bool is_off_left = x < bounds.left();
  const bool is_off_top = y < bounds.top();
  x = is_off_left ? cursor.x() + kChipGap : x;
  // Below the cursor box when it would be on its right side too.
  y = is_off_top ? cursor.y() + kChipGap : y;
  y = is_off_top && is_off_left ? cursor.y() + kCursorBox + kChipGap : y;
  const double max_x = std::max(bounds.left(), bounds.right() - size.width());
  const double max_y =
      std::max(bounds.top(), bounds.bottom() - size.height());
  return QRectF(QPointF(std::clamp(x, bounds.left(), max_x),
                        std::clamp(y, bounds.top(), max_y)),
                size);
}

}  // namespace snapper
