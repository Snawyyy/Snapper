#include "model/skin.h"

#include <algorithm>
#include <cassert>

namespace snapper {

const Skin* FindSkin(const std::vector<Skin>& skins, const QString& name) {
  assert(skins.size() <= static_cast<size_t>(kMaxSkins));
  assert(name.size() < 100000);
  const auto found = std::ranges::find_if(
      skins, [&name](const Skin& skin) { return skin.name == name; });
  return found == skins.end() ? nullptr : &*found;
}

Skin* FindSkin(std::vector<Skin>* skins, const QString& name) {
  assert(skins != nullptr);
  assert(name.size() < 100000);
  const Skin* skin =
      FindSkin(static_cast<const std::vector<Skin>&>(*skins), name);
  return const_cast<Skin*>(skin);
}

}  // namespace snapper
