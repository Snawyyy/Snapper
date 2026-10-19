#ifndef SNAPPER_IO_CHARACTER_JSON_H_
#define SNAPPER_IO_CHARACTER_JSON_H_

#include <QJsonObject>
#include <QJsonValue>

#include "io/json_values.h"
#include "model/character.h"

namespace snapper {

// A character as character.json and a project keep it.
QJsonObject CharacterToJson(const Character& character);
// Reads a character. What breaks the tree (a repeated id, a parent after
// its child) is noted and the character comes back empty, so a damaged
// file never loads half a rig.
Character CharacterFromJson(const QJsonObject& object, JsonIssues* issues);

// A pose as a key keeps it; only what differs from rest is written.
QJsonValue ValueToJson(const CharacterPose& pose);
CharacterPose ValueFromJson(const QJsonValue& value, CharacterPose fallback,
                            JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_CHARACTER_JSON_H_
