#ifndef SNAPPER_IO_DRAFT_JSON_H_
#define SNAPPER_IO_DRAFT_JSON_H_

#include <QJsonObject>
#include <QJsonValue>

#include "io/json_values.h"
#include "model/draft.h"

namespace snapper {

// A shot's draft: its eye, opacity and cels, each cel by frame and the
// path of its PNG.
QJsonObject DraftToJson(const Draft& draft);
// A missing value is an empty draft; cels come back in frame order, one
// per frame.
Draft DraftFromJson(const QJsonValue& value, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_DRAFT_JSON_H_
