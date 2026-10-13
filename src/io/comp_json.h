#ifndef SNAPPER_IO_COMP_JSON_H_
#define SNAPPER_IO_COMP_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/comp.h"

namespace snapper {

// A shot's comp stack. Kinds, blend modes, shapes and knobs are stored
// by name, so a reordered list never changes a finished shot.
QJsonArray CompToJson(const std::vector<CompEffect>& comp);
std::vector<CompEffect> CompFromJson(const QJsonValue& value,
                                     JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_COMP_JSON_H_
