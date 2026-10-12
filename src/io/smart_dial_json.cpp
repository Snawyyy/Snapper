#include "io/smart_dial_json.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

constexpr std::array<const char*, 2> kSwapNames = {"halfway", "at_stop"};

QJsonObject StopToJson(const DialStop& stop) {
  assert(std::isfinite(stop.at));
  assert(stop.poses.size() <= static_cast<size_t>(kMaxDialPieces));
  QJsonObject poses;
  for (const auto& [piece, pose] : stop.poses) {
    poses.insert(piece, ValueToJson(pose));
  }
  QJsonObject drawings;
  for (const auto& [piece, drawing] : stop.drawings) {
    drawings.insert(piece, drawing);
  }
  return QJsonObject{{"name", stop.name},
                     {"at", stop.at},
                     {"poses", poses},
                     {"drawings", drawings}};
}

DialStop StopFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  DialStop stop;
  stop.name = object.value("name").toString();
  const double at = object.value("at").toDouble();
  stop.at = std::isfinite(at) ? std::clamp(at, 0.0, 1.0) : 0.0;
  const QJsonObject poses = object.value("poses").toObject();
  const QJsonObject drawings = object.value("drawings").toObject();
  const bool is_too_many = poses.size() > kMaxDialPieces ||
                           drawings.size() > kMaxDialPieces;
  if (is_too_many) {
    issues->Note(QStringLiteral("a dial stop has too many pieces"));
    return stop;
  }
  for (auto it = poses.begin(); it != poses.end(); ++it) {
    stop.poses[it.key()] = ValueFromJson(it.value(), PiecePose(), issues);
  }
  for (auto it = drawings.begin(); it != drawings.end(); ++it) {
    stop.drawings[it.key()] = it.value().toString();
  }
  return stop;
}

SmartDial DialFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  SmartDial dial;
  dial.name = object.value("name").toString();
  const QJsonArray pieces =
      Bounded(object.value("pieces"), kMaxDialPieces, "dial pieces", issues);
  for (const QJsonValue& item : pieces) {
    const QString piece = item.toString();
    const bool is_new = !piece.isEmpty() &&
                        std::ranges::find(dial.pieces, piece) ==
                            dial.pieces.end();
    if (is_new) {
      dial.pieces.push_back(piece);
    }
  }
  const QJsonArray stops =
      Bounded(object.value("stops"), kMaxDialStops, "dial stops", issues);
  for (const QJsonValue& item : stops) {
    DialStop stop = StopFromJson(item.toObject(), issues);
    const bool is_taken = std::ranges::any_of(
        dial.stops, [&stop](const DialStop& s) { return s.at == stop.at; });
    if (!is_taken) {
      dial.stops.push_back(std::move(stop));
    }
  }
  std::ranges::sort(dial.stops, {}, &DialStop::at);
  const int rest = object.value("rest").toInt();
  dial.rest = std::clamp(rest, 0,
                         std::max(0, static_cast<int>(dial.stops.size()) - 1));
  dial.swap = static_cast<DialSwap>(
      EnumFromJson(object.value("swap"), kSwapNames, issues));
  return dial;
}

}  // namespace

QJsonArray DialsToJson(const std::vector<SmartDial>& dials) {
  assert(dials.size() <= static_cast<size_t>(kMaxSmartDials));
  QJsonArray array;
  for (const SmartDial& dial : dials) {
    QJsonArray pieces;
    for (const QString& piece : dial.pieces) {
      pieces.append(piece);
    }
    QJsonArray stops;
    for (const DialStop& stop : dial.stops) {
      stops.append(StopToJson(stop));
    }
    array.append(QJsonObject{
        {"name", dial.name},
        {"pieces", pieces},
        {"stops", stops},
        {"rest", dial.rest},
        {"swap", EnumToJson(static_cast<int>(dial.swap), kSwapNames)}});
  }
  assert(array.size() == static_cast<qsizetype>(dials.size()));
  return array;
}

std::vector<SmartDial> DialsFromJson(const QJsonValue& value,
                                     JsonIssues* issues) {
  assert(issues != nullptr);
  std::vector<SmartDial> dials;
  const QJsonArray array =
      Bounded(value, kMaxSmartDials, "smart dials", issues);
  for (const QJsonValue& item : array) {
    SmartDial dial = DialFromJson(item.toObject(), issues);
    const bool is_new =
        !dial.name.isEmpty() && FindDial(dials, dial.name) == nullptr;
    if (is_new) {
      dials.push_back(std::move(dial));
    }
  }
  assert(dials.size() <= static_cast<size_t>(kMaxSmartDials));
  return dials;
}

}  // namespace snapper
