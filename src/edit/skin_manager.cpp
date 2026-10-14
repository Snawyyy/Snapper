#include "edit/skin_manager.h"
#include "edit/skin_picks.h"

#include <QStringList>

#include <algorithm>
#include <cassert>
#include <variant>

#include "anim/sampler.h"
#include "anim/skin.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/rig_edits.h"

namespace snapper {
namespace {

Error NoSkin(const QString& doll, const QString& skin) {
  return Error{Tr("%1 has no skin called %2.").arg(doll, skin)};
}

// The project with doll's skin called skin changed by change.
template <typename Change>
Result<Project> WithSkin(const Project& project, const QString& doll,
                         const QString& skin, Change change) {
  assert(!doll.isEmpty());
  assert(!skin.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    Skin* found = FindSkin(&rig->skins, skin);
    if (found == nullptr) {
      return std::unexpected(NoSkin(doll, skin));
    }
    return change(found);
  });
}

// The first of name, "name 2", "name 3"... that skins do not use.
QString FreshSkinName(const std::vector<Skin>& skins, const QString& name) {
  assert(!name.isEmpty());
  assert(skins.size() <= static_cast<size_t>(kMaxSkins));
  QString fresh = name;
  for (int number = 2; FindSkin(skins, fresh) != nullptr; ++number) {
    fresh = QStringLiteral("%1 %2").arg(name).arg(number);
  }
  return fresh;
}

// The drawing piece shows when nothing picks one: the rig's, else
// Krita's.
int RestDrawing(const RigPiece& rig, const ArtPiece& art) {
  assert(art.default_drawing >= -1);
  assert(rig.default_drawing >= -1);
  return rig.default_drawing >= 0 ? rig.default_drawing
                                  : art.default_drawing;
}

// worn with skin added (at the end) or taken out.
SkinSet WithWorn(SkinSet worn, const QString& skin, bool is_on) {
  assert(!skin.isEmpty());
  std::erase(worn.names, skin);
  if (is_on) {
    worn.names.push_back(skin);
  }
  return worn;
}

// project with layer of shot keyed at frame wearing what pick(SkinSet*)
// makes of what it wears there now.
template <typename Pick>
Result<Project> KeyedWorn(const Project& project, ShotId shot, LayerId layer,
                          Frame frame, Pick pick) {
  assert(frame.index() >= 0);
  assert(layer.value() >= 0);
  return KeyedAt(project, TrackRef{shot, TrackKind::kSkin, layer, {}}, frame,
                 SkinSet(), [&](SkinSet* worn) {
                   pick(worn);
                   return Result<void>();
                 });
}

}  // namespace

SkinManager::SkinManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<QString> SkinManager::AddSkin(const QString& doll,
                                     const QString& from) {
  assert(history_ != nullptr);
  assert(from.size() < 100000);
  const Doll* found = FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  const bool is_full =
      found->rig.skins.size() >= static_cast<size_t>(kMaxSkins);
  if (is_full) {
    return std::unexpected(
        Error{Tr("%1 has as many skins as it can hold.").arg(doll)});
  }
  const Skin* source =
      from.isEmpty() ? nullptr : FindSkin(found->rig.skins, from);
  Skin skin = source != nullptr ? *source : Skin();
  skin.name = FreshSkinName(found->rig.skins,
                            source != nullptr ? source->name : Tr("Skin"));
  const QString name = skin.name;
  const auto added = history_->Apply(
      Tr("Add skin %1").arg(name),
      WithRig(history_->current(), doll, [&skin](Rig* rig) -> Result<void> {
        rig->skins.push_back(skin);
        return {};
      }));
  if (!added) {
    return std::unexpected(added.error());
  }
  return name;
}

Result<void> SkinManager::RenameSkin(const QString& doll, const QString& skin,
                                     const QString& to) {
  assert(history_ != nullptr);
  const QString name = to.trimmed();
  const bool is_bad = name.isEmpty() || name.contains(QLatin1Char('/'));
  if (is_bad) {
    return std::unexpected(
        Error{Tr("A skin needs a name without a slash.")});
  }
  if (name == skin) {
    return {};
  }
  auto next = WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
    const bool is_taken = FindSkin(rig->skins, name) != nullptr;
    if (is_taken) {
      return std::unexpected(
          Error{Tr("%1 already has a skin called %2.").arg(doll, name)});
    }
    Skin* found = FindSkin(&rig->skins, skin);
    if (found == nullptr) {
      return std::unexpected(NoSkin(doll, skin));
    }
    found->name = name;
    return {};
  });
  if (next) {
    next = WithDollLayers(std::move(*next), doll, [&](DollLayer* layer) {
      for (Key<SkinSet>& key : layer->skins.keys) {
        std::ranges::replace(key.value.names, skin, name);
      }
    });
  }
  return history_->Apply(Tr("Rename skin %1").arg(skin), std::move(next));
}

Result<void> SkinManager::RemoveSkin(const QString& doll,
                                     const QString& skin) {
  assert(history_ != nullptr);
  assert(!skin.isEmpty());
  const QString why = WhyNoRemove(doll, skin);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  return history_->Apply(
      Tr("Delete skin %1").arg(skin),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const auto gone = std::erase_if(
            rig->skins, [&skin](const Skin& s) { return s.name == skin; });
        return gone > 0 ? Result<void>()
                        : std::unexpected(NoSkin(doll, skin));
      }));
}

Result<void> SkinManager::MoveSkin(const QString& doll, const QString& skin,
                                   int by) {
  assert(history_ != nullptr);
  assert(by == -1 || by == 1);
  return history_->Apply(
      Tr("Move skin %1").arg(skin),
      WithRig(history_->current(), doll, [&](Rig* rig) -> Result<void> {
        const auto found = std::ranges::find(rig->skins, skin, &Skin::name);
        if (found == rig->skins.end()) {
          return std::unexpected(NoSkin(doll, skin));
        }
        const long at = found - rig->skins.begin();
        const long to = at + by;
        const bool is_inside =
            to >= 0 && to < static_cast<long>(rig->skins.size());
        if (!is_inside) {
          return std::unexpected(Error{Tr("It is already at the end.")});
        }
        std::swap(rig->skins[static_cast<size_t>(at)],
                  rig->skins[static_cast<size_t>(to)]);
        return {};
      }));
}

Result<void> SkinManager::SetDrawing(const QString& doll, const QString& skin,
                                     const std::vector<QString>& pieces,
                                     int drawing) {
  assert(history_ != nullptr);
  assert(drawing >= kSkinHidden);
  const Doll* found = FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  if (pieces.empty()) {
    return std::unexpected(Error{Tr("Pick a piece first.")});
  }
  std::map<QString, QString> names;
  for (const QString& piece : pieces) {
    const ArtPiece* art = FindArt(*found, piece);
    if (art == nullptr) {
      return std::unexpected(NoPiece(doll, piece));
    }
    const bool is_drawing =
        drawing >= 0 && drawing < static_cast<int>(art->drawings.size());
    if (is_drawing) {
      names[piece] = DrawingName(*art, drawing);
    } else if (drawing >= 0) {
      return std::unexpected(
          Error{Tr("%1 has no such drawing.").arg(piece)});
    }
  }
  return history_->Apply(
      Tr("Skin %1").arg(skin),
      WithSkin(history_->current(), doll, skin,
               [&](Skin* edited) -> Result<void> {
                 for (const QString& piece : pieces) {
                   if (drawing == kSkinUnset) {
                     edited->drawings.erase(piece);
                   } else if (drawing == kSkinHidden) {
                     edited->drawings[piece] = QString();
                   } else {
                     edited->drawings[piece] = names[piece];
                   }
                 }
                 return {};
               }));
}

Result<int> SkinManager::MakeFromKritaNames(const QString& doll) {
  assert(history_ != nullptr);
  const QString why = WhyNoKritaSkins(doll);
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  const Doll& found = *FindDoll(history_->current(), doll);
  std::vector<Skin> made;
  for (const QString& prefix : SkinPrefixes(found)) {
    const bool is_new = FindSkin(found.rig.skins, prefix) == nullptr;
    const bool has_room = found.rig.skins.size() + made.size() <
                          static_cast<size_t>(kMaxSkins);
    if (!is_new || !has_room) {
      continue;
    }
    Skin skin{prefix, {}};
    const QString head = prefix + QLatin1Char('/');
    for (const ArtPiece& art : found.art.pieces) {
      const RigPiece* rig = FindRig(found.rig, art.name);
      const int rest = rig != nullptr ? RestDrawing(*rig, art) : -1;
      const bool has_rest =
          rest >= 0 && rest < static_cast<int>(art.drawings.size());
      const QString wanted =
          has_rest ? head + DrawingName(art, rest) : QString();
      QString first;
      const int count = static_cast<int>(art.drawings.size());
      for (int i = 0; i < count; ++i) {
        const QString name = DrawingName(art, i);
        const bool is_ours = name.startsWith(head);
        if (is_ours && (first.isEmpty() || name == wanted)) {
          first = name;
        }
      }
      if (!first.isEmpty()) {
        skin.drawings[art.name] = first;
      }
    }
    made.push_back(std::move(skin));
  }
  const int count = static_cast<int>(made.size());
  const auto added = history_->Apply(
      Tr("Skins from Krita names"),
      WithRig(history_->current(), doll, [&made](Rig* rig) -> Result<void> {
        rig->skins.insert(rig->skins.end(), made.begin(), made.end());
        return {};
      }));
  if (!added) {
    return std::unexpected(added.error());
  }
  return count;
}

Result<void> SkinManager::Wear(ShotId shot,
                               const std::vector<LayerId>& layers,
                               LayerId focus, Frame frame,
                               const QString& skin, bool is_stacking,
                               const std::set<Frame>& also) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const std::vector<QString> worn = WornAt(shot, focus, frame);
  const bool is_off = std::ranges::find(worn, skin) != worn.end();
  Result<Project> next = history_->current();
  bool has_skin = false;
  for (const LayerId layer : layers) {
    const Doll* doll = DollOfLayer(*next, shot, layer);
    const bool is_there =
        doll != nullptr && FindSkin(doll->rig.skins, skin) != nullptr;
    if (!is_there) {
      continue;
    }
    has_skin = true;
    std::set<Frame> frames = also;
    frames.insert(frame);
    for (const Frame at : frames) {
      next = KeyedWorn(*next, shot, layer, at, [&](SkinSet* now) {
        *now = is_off ? WithWorn(*now, skin, false)
               : is_stacking ? WithWorn(*now, skin, true)
                             : SkinSet{{skin}};
      });
      if (!next) {
        break;
      }
      next = WithSkinPicksCleared(std::move(*next), shot, layer, at);
      if (!next) {
        break;
      }
    }
    if (!next) {
      break;
    }
  }
  if (!has_skin) {
    return std::unexpected(
        Error{Tr("No picked doll has a skin called %1.").arg(skin)});
  }
  return history_->Apply(
      is_off ? Tr("Take off skin %1").arg(skin) : Tr("Skin %1").arg(skin),
      std::move(next));
}

Result<void> SkinManager::Step(ShotId shot,
                               const std::vector<LayerId>& layers,
                               LayerId focus, Frame frame, int by) {
  assert(history_ != nullptr);
  assert(by == -1 || by == 1);
  const std::vector<Skin> skins = SkinsOf(DollOf(shot, focus));
  if (skins.empty()) {
    return std::unexpected(Error{Tr("That doll has no skins yet.")});
  }
  const std::vector<QString> worn = WornAt(shot, focus, frame);
  int at = -1;
  for (size_t i = 0; i < skins.size(); ++i) {
    const bool is_worn = std::ranges::find(worn, skins[i].name) != worn.end();
    if (is_worn) {
      at = static_cast<int>(i);
      break;
    }
  }
  // -1 is "no skin", between the last skin and the first.
  const int count = static_cast<int>(skins.size());
  const int to = ((at + 1 + by) % (count + 1) + count + 1) % (count + 1) - 1;
  const QString name = to >= 0 ? skins[static_cast<size_t>(to)].name
                               : QString();
  Result<Project> next = history_->current();
  for (const LayerId layer : layers) {
    const Doll* doll = DollOfLayer(*next, shot, layer);
    const bool is_there =
        doll != nullptr &&
        (name.isEmpty() || FindSkin(doll->rig.skins, name) != nullptr);
    if (!is_there) {
      continue;
    }
    next = KeyedWorn(*next, shot, layer, frame, [&](SkinSet* now) {
      *now = name.isEmpty() ? SkinSet() : SkinSet{{name}};
    });
    if (!next) {
      break;
    }
  }
  return history_->Apply(
      name.isEmpty() ? Tr("No skin") : Tr("Skin %1").arg(name),
      std::move(next));
}

Result<void> SkinManager::UpdateFromLook(ShotId shot, LayerId layer,
                                         Frame frame, const QString& skin) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  const DollLayer* posed = PosedLayerOf(project, shot, layer);
  const Doll* doll = DollOfLayer(project, shot, layer);
  if (posed == nullptr || doll == nullptr) {
    return std::unexpected(Error{Tr("Pick a doll first.")});
  }
  if (FindSkin(doll->rig.skins, skin) == nullptr) {
    return std::unexpected(NoSkin(doll->name, skin));
  }
  std::map<QString, QString> taken;
  for (const ArtPiece& art : doll->art.pieces) {
    const auto keyed = posed->pieces.find(art.name);
    const PiecePose hand = keyed == posed->pieces.end()
                               ? PiecePose()
                               : Sample(keyed->second, frame, PiecePose());
    const bool is_hidden = hand.opacity <= 0.0;
    const bool is_swapped =
        hand.drawing >= 0 &&
        hand.drawing < static_cast<int>(art.drawings.size());
    if (is_hidden) {
      taken[art.name] = QString();
    } else if (is_swapped) {
      taken[art.name] = DrawingName(art, hand.drawing);
    }
  }
  if (taken.empty()) {
    return std::unexpected(Error{
        Tr("Swap or hide some pieces at the playhead first; the skin takes "
           "those.")});
  }
  auto next = WithSkin(project, doll->name, skin,
                       [&taken](Skin* edited) -> Result<void> {
                         for (const auto& [piece, name] : taken) {
                           edited->drawings[piece] = name;
                         }
                         return {};
                       });
  for (const auto& [piece, name] : taken) {
    if (!next) {
      break;
    }
    next = KeyedAt(*next, TrackRef{shot, TrackKind::kPiece, layer, piece},
                   frame, PiecePose(), [](PiecePose* pose) {
                     pose->drawing = -1;
                     pose->opacity = pose->opacity <= 0.0 ? 1.0 : pose->opacity;
                     return Result<void>();
                   });
  }
  if (next) {
    next = KeyedWorn(*next, shot, layer, frame, [&skin](SkinSet* now) {
      *now = WithWorn(*now, skin, true);
    });
  }
  return history_->Apply(Tr("Update skin %1").arg(skin), std::move(next));
}

std::vector<Skin> SkinManager::SkinsOf(const QString& doll) const {
  assert(history_ != nullptr);
  assert(doll.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  return found != nullptr ? found->rig.skins : std::vector<Skin>();
}

QString SkinManager::DollOf(ShotId shot, LayerId layer) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Doll* doll = DollOfLayer(history_->current(), shot, layer);
  return doll != nullptr ? doll->name : QString();
}

std::vector<QString> SkinManager::WornAt(ShotId shot, LayerId layer,
                                         Frame frame) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  const DollLayer* posed = PosedLayerOf(project, shot, layer);
  const Doll* doll = DollOfLayer(project, shot, layer);
  if (posed == nullptr || doll == nullptr) {
    return {};
  }
  return SkinsAt(*doll, *posed, frame).names;
}

int SkinManager::DrawingIn(const QString& doll, const QString& skin,
                           const QString& piece) const {
  assert(history_ != nullptr);
  assert(piece.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  const Skin* worn =
      found != nullptr ? FindSkin(found->rig.skins, skin) : nullptr;
  const ArtPiece* art =
      worn != nullptr && !piece.isEmpty() ? FindArt(*found, piece) : nullptr;
  if (art == nullptr) {
    return kSkinUnset;
  }
  const auto named = worn->drawings.find(piece);
  if (named == worn->drawings.end()) {
    return kSkinUnset;
  }
  return named->second.isEmpty() ? kSkinHidden
                                 : FindDrawing(*art, named->second);
}

QString SkinManager::WhyNoRemove(const QString& doll,
                                 const QString& skin) const {
  assert(history_ != nullptr);
  assert(skin.size() < 100000);
  QStringList shots;
  for (const auto& shot : history_->current().shots) {
    const bool is_keyed =
        std::ranges::any_of(shot->layers, [&](const Layer& layer) {
          const auto* posed = std::get_if<DollLayer>(&layer.content);
          return posed != nullptr && posed->doll == doll &&
                 std::ranges::any_of(posed->skins.keys,
                                     [&skin](const Key<SkinSet>& key) {
                                       return std::ranges::find(
                                                  key.value.names, skin) !=
                                              key.value.names.end();
                                     });
        });
    if (is_keyed) {
      shots.append(shot->name);
    }
  }
  return shots.isEmpty()
             ? QString()
             : Tr("Worn in %1: take it off there first.")
                   .arg(shots.join(QStringLiteral(", ")));
}

QString SkinManager::WhyNoKritaSkins(const QString& doll) const {
  assert(history_ != nullptr);
  assert(doll.size() < 100000);
  const Doll* found =
      doll.isEmpty() ? nullptr : FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return Tr("Pick a doll first.");
  }
  for (const QString& prefix : SkinPrefixes(*found)) {
    if (FindSkin(found->rig.skins, prefix) == nullptr) {
      return QString();
    }
  }
  return Tr("Name Krita layers \"skin/drawing\" (\"pajamas/arm\") to make "
            "skins from them; every such name has a skin already.");
}

}  // namespace snapper
