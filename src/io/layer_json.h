#ifndef SNAPPER_IO_LAYER_JSON_H_
#define SNAPPER_IO_LAYER_JSON_H_

#include <QJsonObject>

#include "io/json_values.h"
#include "model/shot.h"

namespace snapper {

QJsonObject ShotToJson(const Shot& shot);
Shot ShotFromJson(const QJsonObject& object, JsonIssues* issues);

// A text layer's lettering (font, size, bold, colors, outline; not its
// words), shared by text layers and the reel's text clips.
void AddLetters(const TextLayer& text, QJsonObject* object);
void ReadLetters(const QJsonObject& object, TextLayer* text,
                 JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_LAYER_JSON_H_
