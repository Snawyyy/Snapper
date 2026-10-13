#ifndef SNAPPER_IO_COMP_FSP_JSON_H_
#define SNAPPER_IO_COMP_FSP_JSON_H_

#include <QJsonObject>
#include <QJsonValue>

#include "io/json_values.h"
#include "model/comp.h"

namespace snapper {

// A plugin effect's values (kFsp, kParam0 on), saved by the plugin's own
// parameter names, so a plugin that reorders its parameters never mixes
// up a file: {"Width": keys, "Center": {"x": keys, "y": keys}}. A name
// the plugin repeats gets " (2)", " (3)" after its later uses.
QJsonObject FspParamsToJson(const CompEffect& effect);
// Reads value into effect, whose kind and plugin are already set. A
// plugin this Snapper lacks keeps value as it is (kept_params), so a
// save writes it back unchanged.
void FspParamsFromJson(const QJsonValue& value, CompEffect* effect,
                       JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_COMP_FSP_JSON_H_
