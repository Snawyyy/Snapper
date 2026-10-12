#ifndef SNAPPER_IO_COLLIDER_JSON_H_
#define SNAPPER_IO_COLLIDER_JSON_H_

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <map>

#include "model/doll.h"

namespace snapper {

// A piece's collider as rig.json keeps it under "collider"; a piece
// without one writes an empty object.
QJsonObject ColliderToJson(const Collider& collider);

// The collider in value. Files from before colliders, or a broken one,
// give none. Dots are kept in ring order, once per slot, and the
// numbers kept in range.
Collider ColliderFromJson(const QJsonValue& value);

// A piece's colliders per drawing as rig.json keeps them under
// "drawing_colliders": an object of drawing name to collider (an empty
// object for a drawing that has none).
QJsonObject DrawingCollidersToJson(const std::map<QString, Collider>& own);

// The colliders per drawing in value; files from before them give none.
// At most kMaxPieceDrawings are kept.
std::map<QString, Collider> DrawingCollidersFromJson(const QJsonValue& value);

}  // namespace snapper

#endif  // SNAPPER_IO_COLLIDER_JSON_H_
