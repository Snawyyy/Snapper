#ifndef SNAPPER_IO_SMART_DIAL_JSON_H_
#define SNAPPER_IO_SMART_DIAL_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/smart_dial.h"

namespace snapper {

QJsonArray DialsToJson(const std::vector<SmartDial>& dials);
// Reads dials, dropping a nameless or repeated dial and putting each
// one's stops in order, so a damaged file still loads.
std::vector<SmartDial> DialsFromJson(const QJsonValue& value,
                                     JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_SMART_DIAL_JSON_H_
