#include "io/own_drawing_json.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cassert>

namespace snapper {

QJsonArray OwnDrawingsToJson(const std::vector<OwnDrawing>& drawings) {
  assert(drawings.size() <= static_cast<size_t>(kMaxPieceDrawings));
  QJsonArray array;
  for (const OwnDrawing& drawing : drawings) {
    QJsonObject object{{"file", drawing.file}, {"name", drawing.name}};
    const bool has_offset = !drawing.offset.isNull();
    if (has_offset) {
      object.insert("offset", QJsonArray{drawing.offset.x(),
                                         drawing.offset.y()});
    }
    array.append(object);
  }
  assert(array.size() == static_cast<qsizetype>(drawings.size()));
  return array;
}

std::vector<OwnDrawing> OwnDrawingsFromJson(const QJsonValue& value,
                                            JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonArray array =
      Bounded(value, kMaxPieceDrawings, "drawings made in Snapper", issues);
  std::vector<OwnDrawing> drawings;
  for (const QJsonValue& item : array) {
    const QJsonObject object = item.toObject();
    OwnDrawing drawing;
    drawing.file = object.value("file").toString();
    drawing.name = object.value("name").toString();
    const QJsonArray offset = object.value("offset").toArray();
    drawing.offset = QPoint(offset.at(0).toInt(), offset.at(1).toInt());
    const bool is_twice = std::any_of(
        drawings.begin(), drawings.end(),
        [&drawing](const OwnDrawing& had) { return had.file == drawing.file; });
    const bool is_valid =
        !drawing.file.isEmpty() && !drawing.name.isEmpty() && !is_twice;
    if (!is_valid) {
      issues->Note(QStringLiteral("a drawing made in Snapper was damaged"));
      continue;
    }
    drawings.push_back(drawing);
  }
  assert(drawings.size() <= static_cast<size_t>(kMaxPieceDrawings));
  return drawings;
}

}  // namespace snapper
