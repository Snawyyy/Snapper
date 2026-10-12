#ifndef SNAPPER_IO_SKIN_JSON_H_
#define SNAPPER_IO_SKIN_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/skin.h"

namespace snapper {

// Skins as rig.json keeps them: a name and piece to drawing name ("" for
// hidden).
QJsonArray SkinsToJson(const std::vector<Skin>& skins);
// Reads skins, dropping a nameless or repeated one, so a damaged file
// still loads.
std::vector<Skin> SkinsFromJson(const QJsonValue& value, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_SKIN_JSON_H_
