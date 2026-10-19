#ifndef SNAPPER_EDIT_CHARACTER_EDITS_H_
#define SNAPPER_EDIT_CHARACTER_EDITS_H_

#include <QPointF>
#include <QString>

#include <span>
#include <vector>

#include "base/error.h"
#include "model/character.h"
#include "model/project.h"

namespace snapper {

// The edits of a character's rig: its parts, bones and smart actions.
// Each changes character in place and keeps it sound, or changes nothing
// and says why. CharacterManager wraps them in undo steps.

// A new character called name: a bone part "Bones" holding a vector part
// "Drawing", as a fresh Moho file starts.
Character NewCharacter(const QString& name);

// The first of name, "name 2", "name 3"... no part of character uses.
QString FreshPartName(const Character& character, const QString& name);

// Adds a part of kind under parent (none: at the top), in front of its
// siblings. Answers its id.
Result<PartId> AddPart(Character* character, PartKind kind,
                       const QString& name, PartId parent);
// Removes ids and everything inside them, with the actions and bindings
// of the bones that go.
Result<void> RemoveParts(Character* character, std::span<const PartId> ids);
Result<void> RenamePart(Character* character, PartId id, const QString& name);
// Moves part (and what is inside it) under parent, place siblings from
// the back (clamped), so drag and drop in the part list is one call.
Result<void> MovePart(Character* character, PartId id, PartId parent,
                      int place);
// A copy of part (and what is inside it) just in front of it, with new
// ids. Answers the copy's id.
Result<PartId> DuplicatePart(Character* character, PartId id);
Result<void> SetVisible(Character* character, PartId id, bool is_visible);
// Sets part's rest place, turn, scale, flip and opacity from rest
// (other fields of rest are ignored).
Result<void> SetPartRest(Character* character, PartId id,
                         const CharacterPart& rest);
// How part follows its bones; bone is used by BindMode::kLayer.
Result<void> SetBind(Character* character, PartId id, BindMode bind,
                     BoneId bone);
// The child a switch shows when not keyed.
Result<void> SetRestPick(Character* character, PartId id, PartId pick);
// Ties points of a vector part to bone (none unties), and sets the part
// to BindMode::kPoints.
Result<void> BindPoints(Character* character, PartId id,
                        std::span<const PointId> points, BoneId bone);

// A new bone in bone part, from base to tip (bone part space), under
// parent (none for a root bone). Answers its id.
Result<BoneId> AddBone(Character* character, PartId bone_part, QPointF base,
                       QPointF tip, BoneId parent);
// Replaces bone's settings by changed (same id). A parent that would
// make a loop or is in another part is refused; turn limits are sorted.
Result<void> SetBone(Character* character, const Bone& changed);
// Places bone at rest from base to tip.
Result<void> SetBoneRest(Character* character, BoneId id, QPointF base,
                         QPointF tip);
// Removes ids: their children hang from their parents, their actions and
// point bindings go.
Result<void> RemoveBones(Character* character, std::span<const BoneId> ids);

// Makes bone a smart bone with an action (or ends it).
Result<void> SetSmart(Character* character, BoneId bone, bool is_smart);
// The stop of bone's action at turn, made if missing. Answers its index.
Result<int> AddActionStop(Character* character, BoneId bone, double turn);
Result<void> SetActionStop(Character* character, BoneId bone, int stop,
                           const CharacterPose& pose);
Result<void> RemoveActionStop(Character* character, BoneId bone, int stop);

// project with change(Character*) run on its character called name.
template <typename Change>
Result<Project> WithCharacter(const Project& project, const QString& name,
                              Change change);

}  // namespace snapper

#include "edit/character_edits_inl.h"

#endif  // SNAPPER_EDIT_CHARACTER_EDITS_H_
