#include "edit/character_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

#include <cassert>
#include <optional>

#include "base/text.h"
#include "edit/character_edits.h"
#include "edit/doll_convert.h"
#include "edit/history_manager.h"
#include "edit/vector_edits.h"
#include "io/character_file.h"

namespace snapper {
namespace {

Error NoVector() { return Error{Tr("Pick a vector part to draw on.")}; }

// The drawing of character's vector part, or an error.
Result<VectorDrawing*> DrawingOf(Character* character, PartId part) {
  assert(character != nullptr);
  assert(part.value() >= 0);
  CharacterPart* found = FindPart(character, part);
  if (found == nullptr || found->kind != PartKind::kVector) {
    return std::unexpected(NoVector());
  }
  return &found->drawing;
}

// Runs a vector edit on part's drawing with the character's id counter.
template <typename Change>
auto OnDrawing(Character* character, PartId part, Change change)
    -> decltype(change(static_cast<VectorDrawing*>(nullptr),
                       static_cast<int*>(nullptr))) {
  assert(character != nullptr);
  assert(part.value() >= 0);
  const Result<VectorDrawing*> drawing = DrawingOf(character, part);
  if (!drawing) {
    return std::unexpected(drawing.error());
  }
  return change(*drawing, &character->next_id);
}

}  // namespace

CharacterManager::CharacterManager(HistoryManager* history,
                                   QString library_folder)
    : history_(history), folder_(std::move(library_folder)) {
  assert(history_ != nullptr);
  assert(!folder_.isEmpty());
}

template <typename Change>
Result<void> CharacterManager::Edit(const QString& character,
                                    const QString& label, Change change) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  return history_->Apply(
      label, WithCharacter(history_->current(), character,
                           [&](Character* edited) -> Result<void> {
                             const auto done = change(edited);
                             if (!done) {
                               return std::unexpected(done.error());
                             }
                             return {};
                           }));
}

QStringList CharacterManager::Available() const {
  assert(!folder_.isEmpty());
  assert(history_ != nullptr);
  return CharacterFolders(folder_);
}

Result<QString> CharacterManager::Create(const QString& name) {
  assert(history_ != nullptr);
  assert(!folder_.isEmpty());
  const QString wanted = name.trimmed().isEmpty() ? Tr("Character") : name.trimmed();
  QString fresh = wanted;
  const QStringList taken = Available();
  for (int number = 2;
       taken.contains(fresh) || FindCharacter(history_->current(), fresh);
       ++number) {
    fresh = QStringLiteral("%1 %2").arg(wanted).arg(number);
  }
  Character character = NewCharacter(fresh);
  character.folder = QDir(folder_).filePath(fresh);
  const Result<void> written = WriteCharacterFolder(character.folder, character);
  if (!written) {
    return std::unexpected(written.error());
  }
  Project next = history_->current();
  if (next.characters.size() >= static_cast<size_t>(kMaxProjectCharacters)) {
    return std::unexpected(Error{Tr("The project has too many characters.")});
  }
  next.characters[fresh] = std::make_shared<const Character>(std::move(character));
  const Result<void> applied = history_->Apply(Tr("New character %1").arg(fresh),
                                               std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return fresh;
}

Result<void> CharacterManager::Import(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const Result<Character> read = ReadCharacterFolder(QDir(folder_).filePath(name));
  if (!read) {
    return std::unexpected(read.error());
  }
  Project next = history_->current();
  const bool is_new = FindCharacter(next, read->name) == nullptr;
  if (is_new && next.characters.size() >= static_cast<size_t>(kMaxProjectCharacters)) {
    return std::unexpected(Error{Tr("The project has too many characters.")});
  }
  next.characters[read->name] = std::make_shared<const Character>(*read);
  return history_->Apply(Tr("Add %1").arg(read->name), std::move(next));
}

Result<void> CharacterManager::FromDoll(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const Project& project = history_->current();
  if (FindDoll(project, name) == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(name)});
  }
  Project only = project;
  std::erase_if(only.dolls, [&name](const auto& entry) {
    return entry.first != name;
  });
  Project converted = WithDollsAsCharacters(std::move(only));
  if (FindCharacter(converted, name) == nullptr) {
    return std::unexpected(
        Error{Tr("%1 is a prop picture and stays one.").arg(name)});
  }
  Project next = project;
  next.dolls.erase(name);
  next.characters[name] = converted.characters.at(name);
  next.shots = converted.shots;
  return history_->Apply(Tr("Make %1 a character").arg(name), std::move(next));
}

Result<void> CharacterManager::SaveToLibrary(const QString& name) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const Character* character = FindCharacter(history_->current(), name);
  if (character == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(name)});
  }
  const QString folder = character->folder.isEmpty()
                             ? QDir(folder_).filePath(name)
                             : character->folder;
  return WriteCharacterFolder(folder, *character);
}

Result<PartId> CharacterManager::AddPart(const QString& character,
                                         PartKind kind, PartId parent) {
  assert(static_cast<int>(kind) < kPartKindCount);
  assert(parent.value() >= 0);
  static const char* const kNames[] = {"Group", "Bones", "Drawing", "Switch",
                                       "Picture"};
  const QString name = Tr(kNames[static_cast<int>(kind)]);
  std::optional<PartId> added;
  const Result<void> done =
      Edit(character, Tr("Add %1").arg(name), [&](Character* edited) {
        const Result<PartId> part = snapper::AddPart(edited, kind, name, parent);
        if (part) {
          added = *part;
        }
        return part;
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return *added;
}

Result<void> CharacterManager::RemoveParts(const QString& character,
                                           std::span<const PartId> ids) {
  assert(ids.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(history_ != nullptr);
  const QString label =
      ids.size() == 1 ? Tr("Remove part") : Tr("Remove %1 parts").arg(ids.size());
  return Edit(character, label, [&](Character* edited) {
    return snapper::RemoveParts(edited, ids);
  });
}

Result<void> CharacterManager::RenamePart(const QString& character, PartId id,
                                          const QString& name) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Rename part to %1").arg(name.trimmed()),
              [&](Character* edited) {
                return snapper::RenamePart(edited, id, name);
              });
}

Result<void> CharacterManager::MovePart(const QString& character, PartId id,
                                        PartId parent, int place) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Move part"), [&](Character* edited) {
    return snapper::MovePart(edited, id, parent, place);
  });
}

Result<PartId> CharacterManager::DuplicatePart(const QString& character,
                                               PartId id) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  std::optional<PartId> added;
  const Result<void> done =
      Edit(character, Tr("Duplicate part"), [&](Character* edited) {
        const Result<PartId> copy = snapper::DuplicatePart(edited, id);
        if (copy) {
          added = *copy;
        }
        return copy;
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return *added;
}

Result<void> CharacterManager::SetVisible(const QString& character, PartId id,
                                          bool is_visible) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, is_visible ? Tr("Show part") : Tr("Hide part"),
              [&](Character* edited) {
                return snapper::SetVisible(edited, id, is_visible);
              });
}

Result<void> CharacterManager::SetPartRest(const QString& character, PartId id,
                                           const CharacterPart& rest) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Place part"), [&](Character* edited) {
    return snapper::SetPartRest(edited, id, rest);
  });
}

Result<void> CharacterManager::SetBind(const QString& character, PartId id,
                                       BindMode bind, BoneId bone) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Bind part"), [&](Character* edited) {
    return snapper::SetBind(edited, id, bind, bone);
  });
}

Result<void> CharacterManager::SetRestPick(const QString& character, PartId id,
                                           PartId pick) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Pick switch part"), [&](Character* edited) {
    return snapper::SetRestPick(edited, id, pick);
  });
}

Result<void> CharacterManager::BindPoints(const QString& character, PartId id,
                                          std::span<const PointId> points,
                                          BoneId bone) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character,
              bone.IsValid() ? Tr("Bind points") : Tr("Unbind points"),
              [&](Character* edited) {
                return snapper::BindPoints(edited, id, points, bone);
              });
}

Result<PartId> CharacterManager::AddImagePart(const QString& character,
                                              PartId parent,
                                              const QString& path) {
  assert(!path.isEmpty());
  assert(history_ != nullptr);
  const Character* found = FindCharacter(history_->current(), character);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(character)});
  }
  QImageReader reader(path);
  const QSize size = reader.canRead() ? reader.size() : QSize();
  if (size.isEmpty()) {
    return std::unexpected(
        Error{Tr("%1 isn't a picture Snapper can read.").arg(path)});
  }
  const QString folder =
      found->folder.isEmpty() ? QDir(folder_).filePath(character) : found->folder;
  QDir().mkpath(folder);
  const QString file = QFileInfo(path).fileName();
  const QString copy = QDir(folder).filePath(file);
  const bool is_same = QFileInfo(copy).absoluteFilePath() ==
                       QFileInfo(path).absoluteFilePath();
  if (!is_same) {
    QFile::remove(copy);
    if (!QFile::copy(path, copy)) {
      return std::unexpected(Error{Tr("Could not copy %1.").arg(path)});
    }
  }
  std::optional<PartId> added;
  const Result<void> done =
      Edit(character, Tr("Add picture"), [&](Character* edited) -> Result<void> {
        edited->folder = folder;
        const Result<PartId> part = snapper::AddPart(
            edited, PartKind::kImage, QFileInfo(path).completeBaseName(), parent);
        if (!part) {
          return std::unexpected(part.error());
        }
        CharacterPart* image = FindPart(edited, *part);
        image->image = file;
        image->image_size = QSizeF(size);
        image->offset = QPointF(-size.width() / 2.0, -size.height() / 2.0);
        image->origin = QPointF(size.width() / 2.0, size.height() / 2.0);
        added = *part;
        return {};
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return *added;
}

Result<std::vector<PointId>> CharacterManager::AddCurve(
    const QString& character, PartId part, std::span<const QPointF> places,
    bool is_closed, double curvature) {
  assert(places.size() <= static_cast<size_t>(kMaxCurvePoints) * 100);
  assert(history_ != nullptr);
  std::vector<PointId> ids;
  const Result<void> done =
      Edit(character, Tr("Draw line"), [&](Character* edited) {
        return OnDrawing(edited, part, [&](VectorDrawing* drawing, int* next) {
          const auto added = snapper::AddCurve(drawing, next, places, is_closed,
                                               curvature, kWeldReach);
          if (added) {
            ids = *added;
          }
          return added;
        });
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return ids;
}

Result<PointId> CharacterManager::ExtendFrom(const QString& character,
                                             PartId part, PointId from,
                                             QPointF at) {
  assert(from.value() >= 0);
  assert(history_ != nullptr);
  PointId added;
  const Result<void> done =
      Edit(character, Tr("Add point"), [&](Character* edited) {
        return OnDrawing(edited, part, [&](VectorDrawing* drawing, int* next) {
          const auto point =
              snapper::ExtendFrom(drawing, next, from, at, kSmoothCurvature);
          if (point) {
            added = *point;
          }
          return point;
        });
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return added;
}

Result<void> CharacterManager::TransformPoints(const QString& character,
                                               PartId part,
                                               std::span<const PointId> ids,
                                               const QTransform& transform) {
  assert(transform.isAffine());
  assert(history_ != nullptr);
  return Edit(character, Tr("Move points"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::TransformPoints(drawing, ids, transform);
    });
  });
}

Result<void> CharacterManager::SetCurvature(const QString& character,
                                            PartId part,
                                            std::span<const PointId> ids,
                                            double curvature) {
  assert(std::isfinite(curvature));
  assert(history_ != nullptr);
  return Edit(character, Tr("Curvature"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::SetCurvature(drawing, ids, curvature);
    });
  });
}

Result<void> CharacterManager::SetWidth(const QString& character, PartId part,
                                        std::span<const PointId> ids,
                                        double width) {
  assert(std::isfinite(width));
  assert(history_ != nullptr);
  return Edit(character, Tr("Line width"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::SetWidth(drawing, ids, width);
    });
  });
}

Result<void> CharacterManager::DeletePoints(const QString& character,
                                            PartId part,
                                            std::span<const PointId> ids) {
  assert(ids.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(history_ != nullptr);
  return Edit(character, Tr("Delete points"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::DeletePoints(drawing, ids);
    });
  });
}

Result<void> CharacterManager::Weld(const QString& character, PartId part,
                                    PointId keep, PointId drop) {
  assert(keep.value() >= 0 && drop.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Weld points"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::Weld(drawing, keep, drop);
    });
  });
}

Result<int> CharacterManager::MakeShape(const QString& character, PartId part,
                                        std::span<const PointId> ids,
                                        const VectorShape& style) {
  assert(ids.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(history_ != nullptr);
  int made = -1;
  const Result<void> done =
      Edit(character, Tr("Make shape"), [&](Character* edited) {
        return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
          const Result<int> shape = snapper::MakeShape(drawing, ids, style);
          if (shape) {
            made = *shape;
          }
          return shape;
        });
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return made;
}

Result<void> CharacterManager::SetShapeStyle(const QString& character,
                                             PartId part, int shape,
                                             const VectorShape& style) {
  assert(shape >= -1);
  assert(history_ != nullptr);
  return Edit(character, Tr("Shape look"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::SetShapeStyle(drawing, shape, style);
    });
  });
}

Result<void> CharacterManager::RemoveShape(const QString& character,
                                           PartId part, int shape) {
  assert(shape >= -1);
  assert(history_ != nullptr);
  return Edit(character, Tr("Remove shape"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::RemoveShape(drawing, shape);
    });
  });
}

Result<void> CharacterManager::RaiseShape(const QString& character,
                                          PartId part, int shape, int by) {
  assert(shape >= -1);
  assert(history_ != nullptr);
  return Edit(character, by > 0 ? Tr("Raise shape") : Tr("Lower shape"),
              [&](Character* edited) {
                return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
                  return snapper::RaiseShape(drawing, shape, by);
                });
              });
}

Result<BoneId> CharacterManager::AddBone(const QString& character,
                                         PartId bone_part, QPointF base,
                                         QPointF tip, BoneId parent) {
  assert(bone_part.value() >= 0);
  assert(history_ != nullptr);
  BoneId added;
  const Result<void> done =
      Edit(character, Tr("Add bone"), [&](Character* edited) {
        const Result<BoneId> bone =
            snapper::AddBone(edited, bone_part, base, tip, parent);
        if (bone) {
          added = *bone;
        }
        return bone;
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return added;
}

Result<void> CharacterManager::SetBone(const QString& character,
                                       const Bone& changed) {
  assert(changed.id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Change bone %1").arg(changed.name),
              [&](Character* edited) {
                return snapper::SetBone(edited, changed);
              });
}

Result<void> CharacterManager::SetBoneRest(const QString& character, BoneId id,
                                           QPointF base, QPointF tip) {
  assert(id.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Move bone"), [&](Character* edited) {
    return snapper::SetBoneRest(edited, id, base, tip);
  });
}

Result<void> CharacterManager::RemoveBones(const QString& character,
                                           std::span<const BoneId> ids) {
  assert(ids.size() <= static_cast<size_t>(kMaxCharacterBones));
  assert(history_ != nullptr);
  return Edit(character,
              ids.size() == 1 ? Tr("Remove bone")
                              : Tr("Remove %1 bones").arg(ids.size()),
              [&](Character* edited) {
                return snapper::RemoveBones(edited, ids);
              });
}

Result<void> CharacterManager::SetSmart(const QString& character, BoneId bone,
                                        bool is_smart) {
  assert(bone.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character,
              is_smart ? Tr("Make smart bone") : Tr("End smart bone"),
              [&](Character* edited) {
                return snapper::SetSmart(edited, bone, is_smart);
              });
}

Result<int> CharacterManager::AddActionStop(const QString& character,
                                            BoneId bone, double turn) {
  assert(bone.value() >= 0);
  assert(history_ != nullptr);
  int stop = -1;
  const Result<void> done =
      Edit(character, Tr("Add action stop"), [&](Character* edited) {
        const Result<int> added = snapper::AddActionStop(edited, bone, turn);
        if (added) {
          stop = *added;
        }
        return added;
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return stop;
}

Result<void> CharacterManager::SetActionStop(const QString& character,
                                             BoneId bone, int stop,
                                             const CharacterPose& pose) {
  assert(bone.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Pose action stop"), [&](Character* edited) {
    return snapper::SetActionStop(edited, bone, stop, pose);
  });
}

Result<void> CharacterManager::RemoveActionStop(const QString& character,
                                                BoneId bone, int stop) {
  assert(bone.value() >= 0);
  assert(history_ != nullptr);
  return Edit(character, Tr("Remove action stop"), [&](Character* edited) {
    return snapper::RemoveActionStop(edited, bone, stop);
  });
}

}  // namespace snapper
