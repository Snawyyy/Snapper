#ifndef SNAPPER_IO_COLLIDER_JSON_H_
#define SNAPPER_IO_COLLIDER_JSON_H_

#include <QJsonObject>
#include <QJsonValue>

#include "model/doll.h"

namespace snapper {

// A piece's collider as rig.json keeps it under "collider"; a piece
// without one writes an empty object.
QJsonObject ColliderToJson(const Collider& collider);

// The collider in value. Files from before colliders, or a broken one,
// give none. Dots are kept in ring order, once per slot, and the
// numbers kept in range.
Collider ColliderFromJson(const QJsonValue& value);

}  // namespace snapper

#endif  // SNAPPER_IO_COLLIDER_JSON_H_
