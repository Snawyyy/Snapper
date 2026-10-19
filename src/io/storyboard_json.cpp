#include "io/storyboard_json.h"

#include <QJsonArray>

#include <algorithm>
#include <cassert>

namespace snapper {

void AddStoryboardFields(const StoryboardLayer& board, QJsonObject* object) {
  assert(object != nullptr);
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  QJsonArray cels;
  for (const StoryboardCel& cel : board.cels) {
    cels.append(QJsonObject{{"at", cel.at.index()}, {"path", cel.path}});
  }
  object->insert("opacity", board.opacity);
  object->insert("exported", board.is_exported);
  object->insert("cels", cels);
}

StoryboardLayer StoryboardFromJson(const QJsonObject& object,
                                   JsonIssues* issues) {
  assert(issues != nullptr);
  StoryboardLayer board;
  board.opacity = std::clamp(
      object.value("opacity").toDouble(kDefaultStoryboardOpacity), 0.0, 1.0);
  board.is_exported = object.value("exported").toBool(false);
  const QJsonArray cels =
      Bounded(object.value("cels"), kMaxStoryboardCels, "storyboard cels",
              issues);
  for (const QJsonValue& item : cels) {
    const QJsonObject cel = item.toObject();
    const Frame at(cel.value("at").toInt());
    const QString path = cel.value("path").toString();
    const bool is_usable =
        !path.isEmpty() && CelStartingAt(board, at) == nullptr;
    if (is_usable) {
      board.cels.push_back(StoryboardCel{at, path});
    }
  }
  SortCels(&board);
  assert(board.cels.size() <= static_cast<size_t>(kMaxStoryboardCels));
  return board;
}

}  // namespace snapper
