#ifndef SNAPPER_IO_DOT_GROUP_JSON_H_
#define SNAPPER_IO_DOT_GROUP_JSON_H_

#include <QJsonArray>
#include <QJsonValue>

#include <vector>

#include "io/json_values.h"
#include "model/dot_group.h"

namespace snapper {

QJsonArray DotGroupsToJson(const std::vector<DotGroup>& groups);
// Reads groups, dropping a nameless or repeated group and any dot that
// is repeated or has no point, so a damaged file still loads.
std::vector<DotGroup> DotGroupsFromJson(const QJsonValue& value,
                                        JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_DOT_GROUP_JSON_H_
