#ifndef SNAPPER_IO_OWN_DRAWING_JSON_H_
#define SNAPPER_IO_OWN_DRAWING_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/doll.h"

namespace snapper {

// A piece's drawings made in Snapper, as rig.json keeps them.
QJsonArray OwnDrawingsToJson(const std::vector<OwnDrawing>& drawings);
// Reads them back, dropping one without a file or a name and a file
// listed twice, so a damaged rig still loads.
std::vector<OwnDrawing> OwnDrawingsFromJson(const QJsonValue& value,
                                            JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_OWN_DRAWING_JSON_H_
