#ifndef SNAPPER_IO_DOLL_JSON_H_
#define SNAPPER_IO_DOLL_JSON_H_

#include <QJsonObject>

#include "io/json_values.h"
#include "model/doll.h"

namespace snapper {

// The shapes of art.json and rig.json, shared by the doll files and by
// project files, which carry a copy of each doll they use.
QJsonObject ArtToJson(const DollArt& art);
DollArt ArtFromJson(const QJsonObject& object, JsonIssues* issues);
QJsonObject RigToJson(const Rig& rig);
Rig RigFromJson(const QJsonObject& object, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_DOLL_JSON_H_
