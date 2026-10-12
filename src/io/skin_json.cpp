#include "io/skin_json.h"

#include <QJsonObject>

#include <cassert>

namespace snapper {
namespace {

// A skin touches at most every piece a doll can have.
constexpr int kMaxSkinPieces = 128;

}  // namespace

QJsonArray SkinsToJson(const std::vector<Skin>& skins) {
  assert(skins.size() <= static_cast<size_t>(kMaxSkins));
  QJsonArray array;
  for (const Skin& skin : skins) {
    QJsonObject drawings;
    for (const auto& [piece, drawing] : skin.drawings) {
      drawings.insert(piece, drawing);
    }
    array.append(QJsonObject{{"name", skin.name}, {"drawings", drawings}});
  }
  assert(array.size() == static_cast<qsizetype>(skins.size()));
  return array;
}

std::vector<Skin> SkinsFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  std::vector<Skin> skins;
  const QJsonArray array = Bounded(value, kMaxSkins, "skins", issues);
  for (const QJsonValue& item : array) {
    const QJsonObject object = item.toObject();
    Skin skin;
    skin.name = object.value("name").toString();
    const QJsonObject drawings = object.value("drawings").toObject();
    const bool is_too_many = drawings.size() > kMaxSkinPieces;
    if (is_too_many) {
      issues->Note(QStringLiteral("a skin has too many pieces"));
      continue;
    }
    for (auto it = drawings.begin(); it != drawings.end(); ++it) {
      skin.drawings[it.key()] = it.value().toString();
    }
    const bool is_new =
        !skin.name.isEmpty() && FindSkin(skins, skin.name) == nullptr;
    if (is_new) {
      skins.push_back(std::move(skin));
    }
  }
  assert(skins.size() <= static_cast<size_t>(kMaxSkins));
  return skins;
}

}  // namespace snapper
