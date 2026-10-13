#ifndef SNAPPER_IO_REFERENCE_JSON_H_
#define SNAPPER_IO_REFERENCE_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/reference.h"

namespace snapper {

// A shot's references, by path; a missing file keeps its settings.
QJsonArray ReferencesToJson(const std::vector<Reference>& references);
std::vector<Reference> ReferencesFromJson(const QJsonValue& value,
                                          JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_REFERENCE_JSON_H_
