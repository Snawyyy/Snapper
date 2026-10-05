#ifndef SNAPPER_IO_LAYER_JSON_H_
#define SNAPPER_IO_LAYER_JSON_H_

#include <QJsonObject>

#include "io/json_values.h"
#include "model/shot.h"

namespace snapper {

QJsonObject ShotToJson(const Shot& shot);
Shot ShotFromJson(const QJsonObject& object, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_LAYER_JSON_H_
