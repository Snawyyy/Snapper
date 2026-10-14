#include "anim/skin.h"

#include <algorithm>
#include <cassert>
#include <optional>

#include "anim/sampler.h"

namespace snapper {
namespace {

// The worn skins of doll in its list order: the one that wins first.
std::vector<const Skin*> WornInOrder(const Doll& doll, const SkinSet& worn) {
  assert(doll.rig.skins.size() <= static_cast<size_t>(kMaxSkins));
  assert(worn.names.size() < 100000);
  std::vector<const Skin*> skins;
  for (const Skin& skin : doll.rig.skins) {
    const bool is_worn = std::ranges::find(worn.names, skin.name) !=
                         worn.names.end();
    if (is_worn) {
      skins.push_back(&skin);
    }
  }
  return skins;
}

// The worn skin's version of drawing of art ("pajamas/mouth O"), or -1.
int SkinVersion(const ArtPiece& art, const std::vector<const Skin*>& skins,
                int drawing) {
  assert(drawing >= 0);
  assert(skins.size() <= static_cast<size_t>(kMaxSkins));
  const bool is_known = drawing < static_cast<int>(art.drawings.size());
  if (!is_known) {
    return -1;
  }
  const QString plain = DrawingName(art, drawing);
  for (const Skin* skin : skins) {
    const int found =
        FindDrawing(art, QStringLiteral("%1/%2").arg(skin->name, plain));
    const bool is_found = found >= 0;
    if (is_found) {
      return found;
    }
  }
  return -1;
}

}  // namespace

SkinSet SkinsAt(const Doll& doll, const DollLayer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(IsSorted(layer.skins));
  const SkinSet keyed = Sample(layer.skins, frame, SkinSet());
  SkinSet worn;
  for (const QString& name : keyed.names) {
    const bool is_there = FindSkin(doll.rig.skins, name) != nullptr;
    if (is_there) {
      worn.names.push_back(name);
    }
  }
  return worn;
}

namespace {

// The frame of channel's last key at or before frame; nullopt for none.
template <typename T>
std::optional<Frame> LastKeyAt(const Channel<T>& channel, Frame frame) {
  assert(IsSorted(channel));
  assert(frame.index() >= 0);
  std::optional<Frame> last;
  for (const Key<T>& key : channel.keys) {
    const bool is_past = frame < key.frame;
    if (is_past) {
      break;
    }
    last = key.frame;
  }
  return last;
}

}  // namespace

void DropOlderPicks(const DollLayer& layer, Frame frame,
                    std::map<QString, PiecePose>* poses) {
  assert(poses != nullptr);
  assert(frame.index() >= 0);
  const std::optional<Frame> skin_key = LastKeyAt(layer.skins, frame);
  const bool has_skin_key = skin_key.has_value();
  if (!has_skin_key) {
    return;
  }
  for (const auto& [name, channel] : layer.pieces) {
    const std::optional<Frame> piece_key = LastKeyAt(channel, frame);
    const bool is_older = !piece_key || *piece_key < *skin_key;
    const auto posed = poses->find(name);
    const bool drops = is_older && posed != poses->end();
    if (drops) {
      posed->second.drawing = -1;
    }
  }
}

std::set<QString> HandPicked(const std::map<QString, PiecePose>& poses) {
  assert(poses.size() < 100000);
  std::set<QString> picked;
  for (const auto& [piece, pose] : poses) {
    const bool is_picked = pose.drawing >= 0;
    if (is_picked) {
      picked.insert(piece);
    }
  }
  assert(picked.size() <= poses.size());
  return picked;
}

void ApplySkins(const Doll& doll, const SkinSet& worn,
                const std::set<QString>& hand_picked,
                std::map<QString, PiecePose>* poses) {
  assert(poses != nullptr);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const std::vector<const Skin*> skins = WornInOrder(doll, worn);
  const bool is_bare = skins.empty();
  if (is_bare) {
    return;
  }
  for (const RigPiece& rig : doll.rig.pieces) {
    const ArtPiece* art = FindArt(doll, rig.name);
    const bool is_free = art != nullptr && !hand_picked.contains(rig.name);
    if (!is_free) {
      continue;
    }
    const QString* choice = nullptr;
    for (const Skin* skin : skins) {
      const auto named = skin->drawings.find(rig.name);
      const bool is_named = named != skin->drawings.end();
      if (is_named) {
        choice = &named->second;
        break;
      }
    }
    const bool is_hidden = choice != nullptr && choice->isEmpty();
    if (is_hidden) {
      (*poses)[rig.name].opacity = 0.0;
      continue;
    }
    const auto posed = poses->find(rig.name);
    const int dialed = posed != poses->end() ? posed->second.drawing : -1;
    const bool is_dialed = dialed >= 0;
    if (is_dialed) {
      const int version = SkinVersion(*art, skins, dialed);
      const bool has_version = version >= 0;
      if (has_version) {
        posed->second.drawing = version;
      }
      continue;
    }
    const int chosen = choice != nullptr ? FindDrawing(*art, *choice) : -1;
    const bool is_chosen = chosen >= 0;
    if (is_chosen) {
      (*poses)[rig.name].drawing = chosen;
    }
  }
}

Channel<SkinSet> WornFromStart(const SkinSet& worn) {
  assert(worn.names.size() < 100000);
  Channel<SkinSet> channel;
  [[maybe_unused]] const bool is_set =
      SetKey(&channel, Key<SkinSet>{Frame(0), worn});
  assert(is_set);
  return channel;
}

QString RestDrawingName(const Doll& doll, const QString& skin,
                        const QString& piece) {
  assert(!piece.isEmpty());
  assert(skin.size() < 100000);
  std::map<QString, PiecePose> poses;
  const bool is_skinned = !skin.isEmpty();
  if (is_skinned) {
    ApplySkins(doll, SkinSet{{skin}}, {}, &poses);
  }
  const auto posed = poses.find(piece);
  const int drawing = posed != poses.end() ? posed->second.drawing : -1;
  return ShownDrawingName(doll, piece, drawing);
}

std::vector<QString> SkinPrefixes(const Doll& doll) {
  assert(doll.art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(doll.rig.skins.size() <= static_cast<size_t>(kMaxSkins));
  std::vector<QString> prefixes;
  for (const ArtPiece& art : doll.art.pieces) {
    const int count = static_cast<int>(art.drawings.size());
    for (int i = 0; i < count; ++i) {
      const QString name = DrawingName(art, i);
      const qsizetype slash = name.indexOf(QLatin1Char('/'));
      const bool is_skinned = slash > 0 && slash < name.size() - 1;
      if (!is_skinned) {
        continue;
      }
      const QString prefix = name.left(slash).trimmed();
      const bool is_new = !prefix.isEmpty() &&
                          std::ranges::find(prefixes, prefix) ==
                              prefixes.end();
      if (is_new) {
        prefixes.push_back(prefix);
      }
    }
  }
  return prefixes;
}

}  // namespace snapper
