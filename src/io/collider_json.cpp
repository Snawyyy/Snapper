#include "io/collider_json.h"

#include <QJsonArray>
#include <QString>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
QJsonObject ColliderToJson(const Collider& collider) {
  assert(collider.dots.size() <= static_cast<size_t>(kMaxColliderDots));
  QJsonObject object;
  const bool is_on = collider.IsOn();
  if (!is_on) {
    return object;
  }
  QJsonArray dots;
  for (const ColliderDot& dot : collider.dots) {
    dots.append(QJsonArray{dot.at.x(), dot.at.y(), dot.slot});
  }
  QJsonArray spared;
  for (const QString& name : collider.spared) {
    spared.append(name);
  }
  object.insert("slots", collider.ring_size);
  object.insert("dots", dots);
  object.insert("spared", spared);
  assert(object.contains("dots"));
  return object;
}

Collider ColliderFromJson(const QJsonValue& value) {
  assert(kMaxColliderDots > 0);
  assert(kMaxDollPieces > 0);
  Collider collider;
  const QJsonObject object = value.toObject();
  const int ring_size =
      std::clamp(object.value("slots").toInt(0), 0, kMaxColliderDots);
  const QJsonArray dots = object.value("dots").toArray();
  int last = -1;
  for (const QJsonValue& item : dots) {
    const QJsonArray triple = item.toArray();
    const QPointF at(triple.at(0).toDouble(), triple.at(1).toDouble());
    const int slot = triple.at(2).toInt(-1);
    const bool is_room =
        collider.dots.size() < static_cast<size_t>(kMaxColliderDots);
    const bool is_next = slot > last && slot < ring_size && is_room &&
                         std::isfinite(at.x()) && std::isfinite(at.y());
    if (is_next) {
      collider.dots.push_back({at, slot});
      last = slot;
    }
  }
  const bool is_on = !collider.dots.empty();
  if (!is_on) {
    return Collider();
  }
  collider.ring_size = ring_size;
  // Older files also kept a "depth"; outlines now push by where things
  // were as drawn, so it is not read.
  for (const QJsonValue& name : object.value("spared").toArray()) {
    const bool is_room =
        collider.spared.size() < static_cast<size_t>(kMaxDollPieces);
    if (is_room) {
      collider.spared.push_back(name.toString());
    }
  }
  return collider;
}

}  // namespace snapper
