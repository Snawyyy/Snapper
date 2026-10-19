#ifndef SNAPPER_EDIT_CHARACTER_POSE_EDITS_H_
#define SNAPPER_EDIT_CHARACTER_POSE_EDITS_H_

#include <QPointF>

#include <span>

#include "model/character.h"

namespace snapper {

// Changes to one character pose, shared by posing a layer's key on the
// Pose tab and posing a smart bone's action stop on the Character tab.
// Each answers the changed pose; pure.

// bone turned to turn degrees from rest (within its limits).
CharacterPose WithBoneTurn(const Character& character, CharacterPose pose,
                           BoneId bone, double turn);
// bone's tip pulled toward target (character space) by turning bone and
// its parents (IK), at most chain bones.
CharacterPose WithBoneReach(const Character& character,
                            const CharacterPose& pose, BoneId bone,
                            QPointF target, int chain);
// bone alone turned so it points at target (character space); its
// children follow. How a smart bone or a Shift drag turns one bone.
CharacterPose WithBoneAim(const Character& character, CharacterPose pose,
                          BoneId bone, QPointF target);
// The Pose stage's bone drag: a smart or lone (Shift) bone aims at
// target, any other pulls its chain by IK.
CharacterPose WithBoneDrag(const Character& character,
                           const CharacterPose& pose, BoneId bone,
                           QPointF target, bool is_lone);
// bone's base pushed by by (its bone part's space) on top of its pose.
CharacterPose WithBoneShift(CharacterPose pose, BoneId bone, QPointF by);
// bone stretched to scale.
CharacterPose WithBoneScale(CharacterPose pose, BoneId bone, double scale);
// part moved by by (its parent's space), turned by degrees, scaled by
// scale (each of x and y), on top of its pose.
CharacterPose WithPartShift(CharacterPose pose, PartId part, QPointF by);
CharacterPose WithPartTurn(CharacterPose pose, PartId part, double degrees);
CharacterPose WithPartScale(CharacterPose pose, PartId part, double scale_x,
                            double scale_y);
CharacterPose WithPartOpacity(CharacterPose pose, PartId part, double opacity);
// points of part moved by by (the part's space).
CharacterPose WithPointsShift(CharacterPose pose, PartId part,
                              std::span<const PointId> points, QPointF by);
// switch shows child.
CharacterPose WithPick(CharacterPose pose, PartId switch_part, PartId child);
// What a pose that sits at rest leaves: parts and bones with nothing
// changed are dropped, so keys stay small and compare equal.
CharacterPose Trimmed(CharacterPose pose);

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_POSE_EDITS_H_
