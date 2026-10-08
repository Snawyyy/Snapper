#include "io/point_motion_json.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

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

// The edge an old whole-piece motion hung from.
enum class Edge { kTop, kLeft, kBottom, kRight };
constexpr int kEdgeCount = 4;
constexpr std::array<const char*, kEdgeCount> kEdgeNames = {
    "top", "left", "bottom", "right"};

// An old whole-piece motion, as rig.json kept it under "motion".
struct OldMotion final {
  WarpMotionKind kind = WarpMotionKind::kNone;
  double size = 8.0;
  int cycle = 24;
  Edge edge = Edge::kTop;
};

// How far point is from old's anchor edge, 0 on it to 1 on the far
// side, on a grid that is on.
double Along(const OldMotion& old, WarpGrid grid, int point) {
  assert(grid.IsOn());
  assert(point >= 0 && point < grid.PointCount());
  const double x = static_cast<double>(point % (grid.columns + 1)) /
                   grid.columns;
  const double y = static_cast<double>(point / (grid.columns + 1)) /
                   grid.rows;
  const bool is_level = old.edge == Edge::kTop || old.edge == Edge::kBottom;
  const bool is_near_end = old.edge == Edge::kTop || old.edge == Edge::kLeft;
  const double from_start = is_level ? y : x;
  return is_near_end ? from_start : 1.0 - from_start;
}

// From the middle of the drawing out to point, -1 to 1 each way.
QPointF Outward(WarpGrid grid, int point) {
  assert(grid.IsOn());
  assert(point >= 0 && point < grid.PointCount());
  const QPointF spot(
      static_cast<double>(point % (grid.columns + 1)) / grid.columns,
      static_cast<double>(point / (grid.columns + 1)) / grid.rows);
  return (spot - QPointF(0.5, 0.5)) * 2.0;
}

// The motion that gives point the push old gave it: hanging motions
// swing across their edge, scaled (and for a wave delayed) by how far
// the point hangs; swelling ones push out from the middle.
PointMotion PointOf(const OldMotion& old, WarpGrid grid, int point) {
  assert(old.kind != WarpMotionKind::kNone);
  assert(point >= 0 && point < grid.PointCount());
  PointMotion motion{point, old.kind, old.size, old.cycle, 0.0, 0.0};
  const double along = Along(old, grid, point);
  const bool is_level = old.edge == Edge::kTop || old.edge == Edge::kBottom;
  const QPointF out = Outward(grid, point);
  switch (old.kind) {
    case WarpMotionKind::kNone:
    case WarpMotionKind::kShiver:
      break;
    case WarpMotionKind::kWave:
      motion.size = old.size * along;
      motion.delay = along;
      motion.angle = is_level ? 0.0 : 90.0;
      break;
    case WarpMotionKind::kSway:
      motion.size = old.size * along * along;
      motion.angle = is_level ? 0.0 : 90.0;
      break;
    case WarpMotionKind::kPulse:
    case WarpMotionKind::kBreathe:
      motion.size = old.size * std::hypot(out.x(), out.y());
      motion.angle = std::atan2(out.y(), out.x()) * 180.0 / std::numbers::pi;
      break;
  }
  motion.size = std::min(motion.size, kMaxWarpMotionSize);
  return motion;
}

// Files before point motions moved a piece's whole grid; each point
// gets the motion that keeps its old push, and points that never moved
// get none.
std::vector<PointMotion> FromOldMotion(const QJsonObject& object,
                                       WarpGrid grid, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(grid.columns >= 0 && grid.rows >= 0);
  OldMotion old;
  old.kind = static_cast<WarpMotionKind>(
      EnumFromJson(object.value("kind"), kMotionNames, issues));
  old.edge = static_cast<Edge>(
      EnumFromJson(object.value("edge"), kEdgeNames, issues));
  old.size = Kept(object.value("size"), old.size, 0.0, kMaxWarpMotionSize);
  old.cycle = std::clamp(object.value("cycle").toInt(old.cycle),
                         kMinWarpCycle, kMaxWarpCycle);
  std::vector<PointMotion> motions;
  const bool is_moving = old.kind != WarpMotionKind::kNone;
  for (int point = 0; is_moving && point < grid.PointCount(); ++point) {
    const PointMotion motion = PointOf(old, grid, point);
    const bool has_push = motion.size > 0.0;
    if (has_push) {
      motions.push_back(motion);
    }
  }
  assert(motions.size() <= static_cast<size_t>(kMaxWarpPoints));
  return motions;
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
  const QJsonValue old = piece.value("motion");
  const bool is_old = !piece.contains("motions") && old.isObject();
  if (is_old) {
    return FromOldMotion(old.toObject(), grid, issues);
  }
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
