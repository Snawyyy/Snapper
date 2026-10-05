#include "io/json_values.h"

#include <array>
#include <cmath>

#include "model/doll.h"

namespace snapper {
namespace {

constexpr std::array<const char*, kEaseCount> kEaseNames = {
    "step", "linear", "ease_in", "ease_out", "ease_in_out"};

double Number(const QJsonObject& object, const char* key, double fallback) {
  assert(key != nullptr);
  assert(std::isfinite(fallback));
  const QJsonValue value = object.value(QLatin1String(key));
  const double number = value.toDouble(fallback);
  return std::isfinite(number) ? number : fallback;
}

}  // namespace

void JsonIssues::Note(const QString& problem) {
  assert(!problem.isEmpty());
  assert(first_.size() < 100000);
  const bool is_first = first_.isEmpty();
  if (is_first) {
    first_ = problem;
  }
}

QJsonArray PointToJson(QPointF point) {
  assert(std::isfinite(point.x()));
  assert(std::isfinite(point.y()));
  return QJsonArray{point.x(), point.y()};
}

QPointF PointFromJson(const QJsonValue& value) {
  const QJsonArray pair = value.toArray();
  const double x = pair.at(0).toDouble();
  const double y = pair.at(1).toDouble();
  const bool is_finite = std::isfinite(x) && std::isfinite(y);
  const QPointF point = is_finite ? QPointF(x, y) : QPointF();
  assert(std::isfinite(point.x()));
  assert(std::isfinite(point.y()));
  return point;
}

QJsonArray SizeToJson(QSize size) {
  assert(size.width() >= -1);
  assert(size.height() >= -1);
  return QJsonArray{size.width(), size.height()};
}

QSize SizeFromJson(const QJsonValue& value) {
  const QJsonArray pair = value.toArray();
  const QSize read(pair.at(0).toInt(), pair.at(1).toInt());
  const bool is_valid = read.width() >= 0 && read.height() >= 0;
  const QSize size = is_valid ? read : QSize(0, 0);
  assert(size.width() >= 0);
  assert(size.height() >= 0);
  return size;
}

QString ColorToJson(const QColor& color) {
  assert(color.isValid());
  assert(color.alpha() >= 0);
  return color.name(QColor::HexArgb);
}

QColor ColorFromJson(const QJsonValue& value, QColor fallback,
                     JsonIssues* issues) {
  assert(issues != nullptr);
  assert(fallback.isValid());
  const bool is_missing = value.isUndefined();
  if (is_missing) {
    return fallback;
  }
  const QColor color = QColor::fromString(value.toString());
  const bool is_valid = color.isValid();
  if (!is_valid) {
    issues->Note(QStringLiteral("bad color %1").arg(value.toString()));
    return fallback;
  }
  return color;
}

QString EnumToJson(int value, std::span<const char* const> names) {
  assert(value >= 0);
  assert(static_cast<size_t>(value) < names.size());
  return QLatin1String(names[static_cast<size_t>(value)]);
}

int EnumFromJson(const QJsonValue& value, std::span<const char* const> names,
                 JsonIssues* issues) {
  assert(issues != nullptr);
  assert(!names.empty());
  const QString name = value.toString();
  for (size_t i = 0; i < names.size(); ++i) {
    const bool is_match = name == QLatin1String(names[i]);
    if (is_match) {
      return static_cast<int>(i);
    }
  }
  issues->Note(QStringLiteral("unknown setting \"%1\"").arg(name));
  return 0;
}

QJsonValue ValueToJson(double value) {
  assert(std::isfinite(value));
  assert(!std::isnan(value));
  return value;
}

double ValueFromJson(const QJsonValue& value, double fallback,
                     JsonIssues* issues) {
  assert(issues != nullptr);
  assert(std::isfinite(fallback));
  const double number = value.toDouble(std::nan(""));
  const bool is_number = std::isfinite(number);
  if (!is_number) {
    issues->Note(QStringLiteral("a key has no number"));
  }
  const double read = is_number ? number : fallback;
  assert(std::isfinite(read));
  return read;
}

QJsonValue ValueToJson(const PiecePose& pose) {
  assert(std::isfinite(pose.rotation));
  assert(pose.warp.size() <= static_cast<size_t>(kMaxWarpPoints));
  QJsonArray warp;
  for (const QPointF& point : pose.warp) {
    warp.append(PointToJson(point));
  }
  return QJsonObject{{"r", pose.rotation},       {"x", pose.offset.x()},
                     {"y", pose.offset.y()},     {"sx", pose.scale_x},
                     {"sy", pose.scale_y},       {"k", pose.skew},
                     {"o", pose.opacity},        {"d", pose.drawing},
                     {"w", warp}};
}

PiecePose ValueFromJson(const QJsonValue& value, PiecePose fallback,
                        JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonObject object = value.toObject();
  PiecePose pose = fallback;
  pose.rotation = Number(object, "r", fallback.rotation);
  pose.offset = QPointF(Number(object, "x", fallback.offset.x()),
                        Number(object, "y", fallback.offset.y()));
  pose.scale_x = Number(object, "sx", fallback.scale_x);
  pose.scale_y = Number(object, "sy", fallback.scale_y);
  pose.skew = Number(object, "k", fallback.skew);
  pose.opacity = Number(object, "o", fallback.opacity);
  pose.drawing = object.value("d").toInt(fallback.drawing);
  const QJsonArray warp = object.value("w").toArray();
  const bool is_too_many = warp.size() > kMaxWarpPoints;
  if (is_too_many) {
    issues->Note(QStringLiteral("a warp has too many points"));
    return pose;
  }
  pose.warp.clear();
  for (const QJsonValue& point : warp) {
    pose.warp.push_back(PointFromJson(point));
  }
  assert(pose.warp.size() <= static_cast<size_t>(kMaxWarpPoints));
  return pose;
}

QJsonValue ValueToJson(const CameraPose& pose) {
  assert(std::isfinite(pose.zoom));
  assert(std::isfinite(pose.shake));
  return QJsonObject{{"x", pose.center.x()}, {"y", pose.center.y()},
                     {"z", pose.zoom},       {"r", pose.rotation},
                     {"s", pose.shake}};
}

CameraPose ValueFromJson(const QJsonValue& value, CameraPose fallback,
                         JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonObject object = value.toObject();
  CameraPose pose;
  pose.center = QPointF(Number(object, "x", fallback.center.x()),
                        Number(object, "y", fallback.center.y()));
  pose.zoom = Number(object, "z", fallback.zoom);
  pose.rotation = Number(object, "r", fallback.rotation);
  pose.shake = Number(object, "s", fallback.shake);
  assert(std::isfinite(pose.zoom));
  return pose;
}

QString EaseToJson(Ease ease) {
  assert(static_cast<int>(ease) < kEaseCount);
  assert(kEaseNames.size() == static_cast<size_t>(kEaseCount));
  return EnumToJson(static_cast<int>(ease), kEaseNames);
}

Ease EaseFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kEaseNames.size() == static_cast<size_t>(kEaseCount));
  return static_cast<Ease>(EnumFromJson(value, kEaseNames, issues));
}

}  // namespace snapper
