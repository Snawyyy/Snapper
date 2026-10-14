#include "io/draft_json.h"

#include <QJsonArray>

#include <algorithm>
#include <cassert>

namespace snapper {

QJsonObject DraftToJson(const Draft& draft) {
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  assert(draft.opacity >= 0.0 && draft.opacity <= 1.0);
  QJsonArray cels;
  for (const DraftCel& cel : draft.cels) {
    cels.append(QJsonObject{{"at", cel.at.index()}, {"path", cel.path}});
  }
  return QJsonObject{{"opacity", draft.opacity},
                     {"shown", draft.is_shown},
                     {"cels", cels}};
}

Draft DraftFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  Draft draft;
  const QJsonObject object = value.toObject();
  draft.opacity = std::clamp(
      object.value("opacity").toDouble(kDefaultDraftOpacity), 0.0, 1.0);
  draft.is_shown = object.value("shown").toBool(true);
  const QJsonArray cels =
      Bounded(object.value("cels"), kMaxDraftCels, "draft cels", issues);
  for (const QJsonValue& item : cels) {
    const QJsonObject cel = item.toObject();
    const Frame at(std::clamp(cel.value("at").toInt(), 0, kMaxFrame));
    const QString path = cel.value("path").toString();
    const bool is_usable =
        !path.isEmpty() && CelStartingAt(draft, at) == nullptr;
    if (is_usable) {
      draft.cels.push_back(DraftCel{at, path});
    }
  }
  SortCels(&draft);
  assert(draft.cels.size() <= static_cast<size_t>(kMaxDraftCels));
  return draft;
}

}  // namespace snapper
