#ifndef SNAPPER_IO_STORYBOARD_JSON_H_
#define SNAPPER_IO_STORYBOARD_JSON_H_

#include <QJsonObject>

#include "io/json_values.h"
#include "model/storyboard.h"

namespace snapper {

// A storyboard layer's own fields (the layer's name, eye and the rest
// are written by layer_json): opacity, the export tick, and the cels,
// each by frame and the path of its PNG.
void AddStoryboardFields(const StoryboardLayer& board, QJsonObject* object);
// Reads them back. Missing fields take their defaults; cels come back in
// frame order, one per frame, and one without a path is dropped.
StoryboardLayer StoryboardFromJson(const QJsonObject& object,
                                   JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_STORYBOARD_JSON_H_
