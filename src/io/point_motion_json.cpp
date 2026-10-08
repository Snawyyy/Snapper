#include "io/point_motion_json.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

constexpr std::array<const char*, kWarpMotionKindCount> kMotionNames = {
    "none", "wave", "pulse", "breathe", "sway", "shiver"};

// number read from value, kept from low to high; fallback when it is
// missing or not a number.
double Kept(const QJsonValue& value, double fallback, double low,
            double high) {
  assert(low <= high);
  assert(fallback >= low && fallback <= high);
  const double read = value.toDouble(fallback);
  return std::isfinite(read) ? std::clamp(read, low, high) : fallback;
}

PointMotion MotionFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kMotionNames.size() == static_cast<size_t>(kWarpMotionKindCount));
  const PointMotion fallback;
  PointMotion motion;
  motion.point = object.value("point").toInt(-1);
  motion.kind = static_cast<WarpMotionKind>(
      EnumFromJson(object.value("kind"), kMotionNames, issues));
  motion.size = Kept(object.value("size"), fallback.size, 0.0,
                     kMaxWarpMotionSize);
  motion.cycle = std::clamp(object.value("cycle").toInt(fallback.cycle),
                            kMinWarpCycle, kMaxWarpCycle);
  motion.angle = std::remainder(
      Kept(object.value("angle"), 0.0, -360.0, 360.0), 360.0);
  motion.delay = Kept(object.value("delay"), 0.0, 0.0, 1.0);
  return motion;
}

}  // namespace

QJsonArray PointMotionsToJson(const std::vector<PointMotion>& motions) {
  assert(motions.size() <= static_cast<size_t>(kMaxWarpPoints));
  QJsonArray array;
  for (const PointMotion& motion : motions) {
    array.append(QJsonObject{
        {"point", motion.point},
        {"kind", EnumToJson(static_cast<int>(motion.kind), kMotionNames)},
        {"size", motion.size},
        {"cycle", motion.cycle},
        {"angle", motion.angle},
        {"delay", motion.delay}});
  }
  assert(array.size() == static_cast<qsizetype>(motions.size()));
  return array;
}

std::vector<PointMotion> PointMotionsFromJson(const QJsonObject& piece,
                                              WarpGrid grid,
                                              JsonIssues* issues) {
  assert(issues != nullptr);
  assert(grid.columns >= 0 && grid.rows >= 0);
  std::vector<PointMotion> motions;
  const QJsonArray array = Bounded(piece.value("motions"), kMaxWarpPoints,
                                   "point motions", issues);
  for (const QJsonValue& item : array) {
    const PointMotion motion = MotionFromJson(item.toObject(), issues);
    const bool is_new =
        motion.kind != WarpMotionKind::kNone && motion.point >= 0 &&
        motion.point < grid.PointCount() &&
        std::none_of(motions.begin(), motions.end(),
                     [&motion](const PointMotion& m) {
                       return m.point == motion.point;
                     });
    if (is_new) {
      motions.push_back(motion);
    }
  }
  assert(motions.size() <= static_cast<size_t>(kMaxWarpPoints));
  return motions;
}

}  // namespace snapper
