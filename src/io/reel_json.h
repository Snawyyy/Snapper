#ifndef SNAPPER_IO_REEL_JSON_H_
#define SNAPPER_IO_REEL_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/reel.h"

namespace snapper {

QJsonArray ReelToJson(const Reel& reel);
// A file without a reel (made before it existed) gets the default one.
// Overlapping or empty clips are noted as damage.
Reel ReelFromJson(const QJsonValue& value, JsonIssues* issues);
// The reel's cut markers, kept apart from its tracks; a file without
// them (made before they existed) has none.
QJsonArray MarkersToJson(const std::vector<Frame>& markers);
std::vector<Frame> MarkersFromJson(const QJsonValue& value,
                                   JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_REEL_JSON_H_
