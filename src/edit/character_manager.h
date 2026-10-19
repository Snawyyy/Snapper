#ifndef SNAPPER_EDIT_CHARACTER_MANAGER_H_
#define SNAPPER_EDIT_CHARACTER_MANAGER_H_

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTransform>

#include <span>
#include <vector>

#include "base/error.h"
#include "model/character.h"

namespace snapper {

class HistoryManager;

// Owns every change to a character's rig (the Character tab): its parts,
// its vector drawings, its bones and their bindings, its switches and
// smart actions, and the character library. Each call is one undo step
// named for what it did, or, inside an open EditScope, the live preview
// of a drag.
class CharacterManager final : public QObject {
  Q_OBJECT

 public:
  CharacterManager(HistoryManager* history, QString library_folder);

  // The folder of library characters.
  const QString& folder() const { return folder_; }
  // Names of the library's characters, sorted.
  QStringList Available() const;
  // A new character in the project (a bone part with a vector part
  // inside) and in the library. Answers its name, made unique.
  Result<QString> Create(const QString& name);
  // Copies a library character into the project, replacing one of the
  // same name there.
  Result<void> Import(const QString& name);
  // Makes the project's doll called name a character of the same name,
  // its layers character layers (see doll_convert.h), as one step.
  Result<void> FromDoll(const QString& name);
  // Writes the project's character back to the library.
  Result<void> SaveToLibrary(const QString& name);

  // Parts.
  Result<PartId> AddPart(const QString& character, PartKind kind,
                         PartId parent);
  Result<void> RemoveParts(const QString& character,
                           std::span<const PartId> ids);
  Result<void> RenamePart(const QString& character, PartId id,
                          const QString& name);
  Result<void> MovePart(const QString& character, PartId id, PartId parent,
                        int place);
  Result<PartId> DuplicatePart(const QString& character, PartId id);
  Result<void> SetVisible(const QString& character, PartId id,
                          bool is_visible);
  Result<void> SetPartRest(const QString& character, PartId id,
                           const CharacterPart& rest);
  Result<void> SetBind(const QString& character, PartId id, BindMode bind,
                       BoneId bone);
  Result<void> SetRestPick(const QString& character, PartId id, PartId pick);
  Result<void> BindPoints(const QString& character, PartId id,
                          std::span<const PointId> points, BoneId bone);
  // Adds a picture to an image part: copies it into the character's
  // library folder and sizes the part to it.
  Result<PartId> AddImagePart(const QString& character, PartId parent,
                              const QString& path);

  // Drawing on a vector part.
  Result<std::vector<PointId>> AddCurve(const QString& character, PartId part,
                                        std::span<const QPointF> places,
                                        bool is_closed, double curvature);
  Result<PointId> ExtendFrom(const QString& character, PartId part,
                             PointId from, QPointF at);
  Result<void> TransformPoints(const QString& character, PartId part,
                               std::span<const PointId> ids,
                               const QTransform& transform);
  Result<void> SetCurvature(const QString& character, PartId part,
                            std::span<const PointId> ids, double curvature);
  Result<void> SetWidth(const QString& character, PartId part,
                        std::span<const PointId> ids, double width);
  Result<void> DeletePoints(const QString& character, PartId part,
                            std::span<const PointId> ids);
  Result<void> Weld(const QString& character, PartId part, PointId keep,
                    PointId drop);
  Result<int> MakeShape(const QString& character, PartId part,
                        std::span<const PointId> ids, const VectorShape& style);
  Result<void> SetShapeStyle(const QString& character, PartId part, int shape,
                             const VectorShape& style);
  Result<void> RemoveShape(const QString& character, PartId part, int shape);
  Result<void> RaiseShape(const QString& character, PartId part, int shape,
                          int by);

  // Bones.
  Result<BoneId> AddBone(const QString& character, PartId bone_part,
                         QPointF base, QPointF tip, BoneId parent);
  Result<void> SetBone(const QString& character, const Bone& changed);
  Result<void> SetBoneRest(const QString& character, BoneId id, QPointF base,
                           QPointF tip);
  Result<void> RemoveBones(const QString& character,
                           std::span<const BoneId> ids);

  // Smart bones.
  Result<void> SetSmart(const QString& character, BoneId bone, bool is_smart);
  Result<int> AddActionStop(const QString& character, BoneId bone,
                            double turn);
  Result<void> SetActionStop(const QString& character, BoneId bone, int stop,
                             const CharacterPose& pose);
  Result<void> RemoveActionStop(const QString& character, BoneId bone,
                                int stop);

  // How far a new stroke's end catches an old point, in pixels.
  static constexpr double kWeldReach = 8.0;

 private:
  template <typename Change>
  Result<void> Edit(const QString& character, const QString& label,
                    Change change);

  HistoryManager* history_;
  QString folder_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_MANAGER_H_
