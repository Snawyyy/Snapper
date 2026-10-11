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
// with_layers keeps each layer's own warp dot settings (a project's rig);
// a doll's own file leaves them out, as layers are the project's.
QJsonObject RigToJson(const Rig& rig, bool with_layers = true);
Rig RigFromJson(const QJsonObject& object, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_DOLL_JSON_H_
