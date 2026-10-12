#ifndef SNAPPER_MODEL_SKIN_H_
#define SNAPPER_MODEL_SKIN_H_

#include <QString>

#include <map>
#include <vector>

namespace snapper {

constexpr int kMaxSkins = 32;

// A named look for a doll (school uniform, pajamas, angry face): for
// each piece it touches, which drawing shows, by its Krita layer name
// (see DrawingName) so a re-export in a new order keeps it. An empty
// drawing name hides the piece. Pieces it does not name show what they
// would without it.
struct Skin final {
  QString name;
  std::map<QString, QString> drawings;

  bool operator==(const Skin&) const = default;
};

// The skin called name, or nullptr.
const Skin* FindSkin(const std::vector<Skin>& skins, const QString& name);
Skin* FindSkin(std::vector<Skin>* skins, const QString& name);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SKIN_H_
