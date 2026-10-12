#include "edit/object_manager.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cassert>
#include <set>
#include <variant>

#include "base/text.h"
#include "edit/doll_library_manager.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/link_manager.h"
#include "edit/project_edits.h"
#include "edit/stage_manager.h"
#include "io/doll_file.h"
#include "io/object_file.h"

namespace snapper {
namespace {

// The most pictures one drop makes objects of.
constexpr int kMaxPicturesAtOnce = kMaxProjectDolls;

// A file's name as an object name: no characters a folder name can't
// hold, never empty.
QString CleanName(const QString& path) {
  assert(!path.isEmpty());
  QString name = QFileInfo(path).completeBaseName().simplified();
  for (const QChar bad : QStringLiteral("/\\:*?\"<>|")) {
    name.replace(bad, QLatin1Char(' '));
  }
  name = name.simplified();
  return name.isEmpty() ? Tr("object") : name;
}

}  // namespace

ObjectManager::ObjectManager(HistoryManager* history,
                             DollLibraryManager* library, StageManager* stage,
                             LinkManager* links)
    : history_(history), library_(library), stage_(stage), links_(links) {
  assert(history_ != nullptr && library_ != nullptr);
  assert(stage_ != nullptr && links_ != nullptr);
}

bool ObjectManager::IsObject(const QString& name) const {
  assert(library_ != nullptr);
  assert(!name.isEmpty());
  const QString folder = library_->FolderOf(name);
  const QFileInfo rig(QDir(folder).filePath(QLatin1String(kRigFileName)));
  const bool has_rig = rig.exists();
  if (!has_rig) {
    return false;
  }
  const QDateTime stamp = rig.lastModified();
  const auto known = kinds_.find(name);
  const bool is_known = known != kinds_.end() && known->second.first == stamp;
  if (is_known) {
    return known->second.second;
  }
  const auto read = ReadRig(folder);
  const bool is_object = read.has_value() && read->object.is_object;
  kinds_[name] = {stamp, is_object};
  return is_object;
}

QStringList ObjectManager::Objects() const {
  assert(library_ != nullptr);
  QStringList objects;
  for (const QString& name : library_->Available()) {
    if (IsObject(name)) {
      objects.append(name);
    }
  }
  return objects;
}

QStringList ObjectManager::Dolls() const {
  assert(library_ != nullptr);
  QStringList dolls;
  for (const QString& name : library_->Available()) {
    if (!IsObject(name)) {
      dolls.append(name);
    }
  }
  return dolls;
}

Result<QStringList> ObjectManager::AddToLibrary(const QStringList& paths,
                                                QStringList* notes) {
  assert(library_ != nullptr);
  assert(paths.size() < 100000);
  const QStringList pictures = ObjectPictures(paths);
  const bool has_pictures = !pictures.isEmpty();
  if (!has_pictures) {
    return std::unexpected(
        Error{Tr("Drop PNG, GIF or WebP pictures, or a folder of them.")});
  }
  const bool is_too_many = pictures.size() > kMaxPicturesAtOnce;
  if (is_too_many) {
    return std::unexpected(Error{
        Tr("At most %1 pictures at once.").arg(kMaxPicturesAtOnce)});
  }
  QStringList names;
  for (const QString& picture : pictures) {
    auto frames = ReadObjectFrames(picture);
    if (!frames) {
      return std::unexpected(frames.error());
    }
    // A name no library doll and no project doll has yet.
    const QString base = CleanName(picture);
    QString name = base;
    for (int n = 2; QFileInfo::exists(library_->FolderOf(name)) ||
                    FindDoll(history_->current(), name) != nullptr;
         ++n) {
      name = QStringLiteral("%1 %2").arg(base).arg(n);
    }
    auto wrote = WriteObjectFolder(library_->FolderOf(name), name, *frames);
    if (!wrote) {
      return std::unexpected(wrote.error());
    }
    const bool is_thinned = frames->kept_every > 1 && notes != nullptr;
    if (is_thinned) {
      notes->append(Tr("%1 had %2 frames; kept every %3rd, %4 in all.")
                        .arg(name)
                        .arg(frames->source_count)
                        .arg(frames->kept_every)
                        .arg(frames->frames.size()));
    }
    names.append(name);
  }
  assert(names.size() == pictures.size());
  return names;
}

Result<std::vector<LayerId>> ObjectManager::AddToShot(
    ShotId shot, const QStringList& paths, std::optional<QPointF> at,
    QStringList* notes) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_shot = FindShot(history_->current(), shot) != nullptr;
  if (!has_shot) {
    return std::unexpected(Error{Tr("Add a shot first.")});
  }
  auto names = AddToLibrary(paths, notes);
  if (!names) {
    return std::unexpected(names.error());
  }
  const QString label = names->size() == 1
                            ? Tr("Add %1").arg(names->front())
                            : Tr("Add %1 objects").arg(names->size());
  EditScope batch(history_, label);
  std::vector<LayerId> layers;
  for (const QString& name : *names) {
    auto imported = library_->Import(name);
    auto layer = imported ? stage_->AddDoll(shot, name)
                          : Result<LayerId>(std::unexpected(imported.error()));
    if (!layer) {
      batch.Cancel();
      return std::unexpected(layer.error());
    }
    layers.push_back(*layer);
    const bool is_placed = at.has_value();
    if (!is_placed) {
      continue;
    }
    auto moved = history_->Apply(
        label, WithLayer(history_->current(), shot, *layer, [&](Layer* edited) {
          Key<PiecePose> key;
          key.value.offset = *at;
          SetKey(&edited->transform, key);
          return Result<void>();
        }));
    if (!moved) {
      batch.Cancel();
      return std::unexpected(moved.error());
    }
  }
  return layers;
}

bool ObjectManager::IsObjectLayer(ShotId shot, LayerId layer) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Doll* doll =
      layer.IsValid() ? DollOfLayer(history_->current(), shot, layer)
                      : nullptr;
  return doll != nullptr && doll->rig.object.is_object;
}

std::vector<LayerId> ObjectManager::ObjectLayers(
    ShotId shot, const std::vector<LayerId>& layers) const {
  assert(layers.size() < 100000);
  std::vector<LayerId> objects;
  for (const LayerId layer : layers) {
    if (IsObjectLayer(shot, layer)) {
      objects.push_back(layer);
    }
  }
  return objects;
}

bool ObjectManager::WouldLoop(const Shot& shot, LayerId object,
                              const LinkEnd& holder) const {
  assert(object.IsValid());
  assert(shot.holds.size() <= static_cast<size_t>(kMaxHoldsPerShot));
  std::vector<LayerId> waiting = {holder.layer};
  std::set<LayerId> seen;
  for (int step = 0; !waiting.empty() && step <= kMaxHoldsPerShot * 2;
       ++step) {
    const LayerId at = waiting.back();
    waiting.pop_back();
    const bool is_object = at == object;
    if (is_object) {
      return true;
    }
    const bool is_new = seen.insert(at).second;
    if (!is_new) {
      continue;
    }
    for (const snapper::Hold& hold : shot.holds) {
      const bool is_carrier = hold.object == at && !hold.IsDrop();
      if (is_carrier) {
        waiting.push_back(hold.holder.layer);
      }
    }
  }
  return false;
}

QString ObjectManager::WhyNoHold(ShotId shot,
                                 const std::vector<LayerId>& objects,
                                 const LinkEnd& holder) const {
  assert(history_ != nullptr);
  assert(objects.size() < 100000);
  const Project& project = history_->current();
  const Shot* found = FindShot(project, shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return Tr("That shot no longer exists.");
  }
  const bool has_objects =
      !objects.empty() &&
      std::ranges::all_of(objects, [&](LayerId object) {
        return IsObjectLayer(shot, object);
      });
  if (!has_objects) {
    return Tr("Pick an object first.");
  }
  const Doll* doll = holder.layer.IsValid()
                         ? DollOfLayer(project, shot, holder.layer)
                         : nullptr;
  const bool has_holder =
      doll != nullptr &&
      (holder.piece.isEmpty() || FindRig(doll->rig, holder.piece) != nullptr);
  if (!has_holder) {
    return Tr("Only a doll on the stage can hold.");
  }
  const bool is_self = std::ranges::find(objects, holder.layer) !=
                       objects.end();
  if (is_self) {
    return Tr("An object can't hold itself.");
  }
  const bool is_loop = std::ranges::any_of(objects, [&](LayerId object) {
    return WouldLoop(*found, object, holder);
  });
  if (is_loop) {
    return Tr("%1 is held by what is picked; that would loop.")
        .arg(links_->NameOf(shot, holder));
  }
  const bool is_full = found->holds.size() + objects.size() >
                       static_cast<size_t>(kMaxHoldsPerShot);
  if (is_full) {
    return Tr("A shot holds at most %1 hold changes.").arg(kMaxHoldsPerShot);
  }
  return QString();
}

Result<void> ObjectManager::Hold(ShotId shot,
                                 const std::vector<LayerId>& objects,
                                 const LinkEnd& holder, Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoHold(shot, objects, holder);
  const bool can_hold = why_not.isEmpty();
  if (!can_hold) {
    return std::unexpected(Error{why_not});
  }
  return history_->Apply(
      Tr("Hold %1").arg(NamesOf(shot, objects)),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        for (const LayerId object : objects) {
          std::erase_if(edited->holds, [&](const snapper::Hold& hold) {
            return hold.object == object && hold.at == at;
          });
          edited->holds.push_back(snapper::Hold{object, holder, at});
        }
        return Result<void>();
      }));
}

QString ObjectManager::WhyNoDrop(ShotId shot,
                                 const std::vector<LayerId>& objects,
                                 Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool is_held =
      !objects.empty() && std::ranges::all_of(objects, [&](LayerId object) {
        return HolderAt(shot, object, at).has_value();
      });
  return is_held ? QString() : Tr("Nothing picked is held at this frame.");
}

Result<void> ObjectManager::Drop(ShotId shot,
                                 const std::vector<LayerId>& objects,
                                 Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoDrop(shot, objects, at);
  const bool can_drop = why_not.isEmpty();
  if (!can_drop) {
    return std::unexpected(Error{why_not});
  }
  return history_->Apply(
      Tr("Drop %1").arg(NamesOf(shot, objects)),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        for (const LayerId object : objects) {
          std::erase_if(edited->holds, [&](const snapper::Hold& hold) {
            return hold.object == object && hold.at == at;
          });
          // A drop on the frame a hold began is no hold at all.
          const snapper::Hold* before = HoldAt(*edited, object, at);
          const bool is_held_before = before != nullptr && !before->IsDrop();
          if (is_held_before) {
            edited->holds.push_back(snapper::Hold{object, LinkEnd{}, at});
          }
        }
        return Result<void>();
      }));
}

QString ObjectManager::WhyNoUndoChange(ShotId shot,
                                       const std::vector<LayerId>& objects,
                                       Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_change =
      found != nullptr &&
      std::ranges::any_of(found->holds, [&](const snapper::Hold& hold) {
        return hold.at == at &&
               std::ranges::find(objects, hold.object) != objects.end();
      });
  return has_change ? QString()
                    : Tr("Nothing picked is held or dropped on this frame.");
}

Result<void> ObjectManager::RemoveChange(ShotId shot,
                                         const std::vector<LayerId>& objects,
                                         Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoUndoChange(shot, objects, at);
  const bool can_remove = why_not.isEmpty();
  if (!can_remove) {
    return std::unexpected(Error{why_not});
  }
  return history_->Apply(
      Tr("Remove hold change"),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        std::erase_if(edited->holds, [&](const snapper::Hold& hold) {
          return hold.at == at &&
                 std::ranges::find(objects, hold.object) != objects.end();
        });
        return Result<void>();
      }));
}

std::optional<LinkEnd> ObjectManager::HolderAt(ShotId shot, LayerId object,
                                               Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const snapper::Hold* hold =
      found != nullptr && object.IsValid() ? HoldAt(*found, object, at)
                                           : nullptr;
  const bool is_held = hold != nullptr && !hold->IsDrop();
  return is_held ? std::optional<LinkEnd>(hold->holder) : std::nullopt;
}

QStringList ObjectManager::HoldTrack(ShotId shot, LayerId object) const {
  assert(history_ != nullptr);
  assert(links_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  QStringList track;
  const bool has_shot = found != nullptr && object.IsValid();
  if (!has_shot) {
    return track;
  }
  for (const snapper::Hold* hold : HoldsOf(*found, object)) {
    track.append(hold->IsDrop()
                     ? Tr("%1: dropped").arg(hold->at.index())
                     : Tr("%1: held by %2")
                           .arg(hold->at.index())
                           .arg(links_->NameOf(shot, hold->holder)));
  }
  return track;
}

QString ObjectManager::WhyNoPlay(ShotId shot,
                                 const std::vector<LayerId>& objects) const {
  assert(objects.size() < 100000);
  const bool has_objects =
      !objects.empty() &&
      std::ranges::all_of(objects, [&](LayerId object) {
        return IsObjectLayer(shot, object);
      });
  return has_objects ? QString() : Tr("Pick an object first.");
}

Result<void> ObjectManager::SetPlay(ShotId shot,
                                    const std::vector<LayerId>& objects,
                                    const ObjectPlay& play) {
  assert(history_ != nullptr);
  assert(objects.size() < 100000);
  const QString why_not = WhyNoPlay(shot, objects);
  const bool is_valid =
      static_cast<int>(play.mode) >= 0 &&
      static_cast<int>(play.mode) < kPlayModeCount &&
      play.frames_per_drawing >= 0 &&
      play.frames_per_drawing <= kMaxFramesPerDrawing && play.offset >= 0 &&
      play.offset <= kMaxObjectFrames * 2;
  if (!why_not.isEmpty() || !is_valid) {
    return std::unexpected(
        Error{why_not.isEmpty() ? Tr("Those play settings are out of range.")
                                : why_not});
  }
  return history_->Apply(
      Tr("Play %1").arg(NamesOf(shot, objects)),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        for (const LayerId object : objects) {
          Layer* layer = FindLayer(edited, object);
          auto* doll = layer != nullptr ? std::get_if<DollLayer>(&layer->content)
                                        : nullptr;
          if (doll != nullptr) {
            doll->play = play;
          }
        }
        return Result<void>();
      }));
}

ObjectPlay ObjectManager::PlayOf(ShotId shot, LayerId object) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const DollLayer* doll =
      object.IsValid() ? PosedLayerOf(history_->current(), shot, object)
                       : nullptr;
  return doll != nullptr ? doll->play : ObjectPlay();
}

int ObjectManager::FrameCount(ShotId shot, LayerId object) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Doll* doll = object.IsValid()
                         ? DollOfLayer(history_->current(), shot, object)
                         : nullptr;
  return doll != nullptr ? static_cast<int>(doll->rig.object.frame_ms.size())
                         : 0;
}

QString ObjectManager::NamesOf(ShotId shot,
                               const std::vector<LayerId>& objects) const {
  assert(links_ != nullptr);
  assert(objects.size() < 100000);
  QStringList names;
  for (const LayerId object : objects) {
    names.append(links_->NameOf(shot, LinkEnd{object, QString()}));
  }
  return names.join(QStringLiteral(", "));
}

}  // namespace snapper
