#include "edit/character_pose_edits.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/bone_ik.h"
#include "anim/character_pose.h"

namespace snapper {

CharacterPose WithBoneTurn(const Character& character, CharacterPose pose,
                           BoneId bone, double turn) {
  assert(std::isfinite(turn));
  assert(bone.value() >= 0);
  const Bone* found = FindBone(character, bone);
  if (found == nullptr) {
    return pose;
  }
  pose.bones[bone].turn = LimitTurn(*found, turn);
  return pose;
}

CharacterPose WithBoneReach(const Character& character,
                            const CharacterPose& pose, BoneId bone,
                            QPointF target, int chain) {
  assert(std::isfinite(target.x()));
  assert(chain >= 0);
  if (FindBone(character, bone) == nullptr) {
    return pose;
  }
  return ReachFor(character, pose, bone, target, chain);
}

CharacterPose WithBoneAim(const Character& character, CharacterPose pose,
                          BoneId bone, QPointF target) {
  assert(std::isfinite(target.x()));
  assert(bone.value() >= 0);
  const Bone* rest = FindBone(character, bone);
  if (rest == nullptr) {
    return pose;
  }
  for (const PosedBone& posed : PoseBones(character, pose)) {
    if (posed.id != bone) {
      continue;
    }
    const double wanted = std::atan2(target.y() - posed.base.y(),
                                     target.x() - posed.base.x()) *
                          180.0 / std::numbers::pi;
    const double turn = std::remainder(wanted - posed.angle, 360.0);
    pose.bones[bone].turn = LimitTurn(*rest, pose.bones[bone].turn + turn);
  }
  return pose;
}

CharacterPose WithBoneDrag(const Character& character,
                           const CharacterPose& pose, BoneId bone,
                           QPointF target, bool is_lone) {
  assert(std::isfinite(target.x()));
  assert(bone.value() >= 0);
  const Bone* rest = FindBone(character, bone);
  if (rest == nullptr) {
    return pose;
  }
  return is_lone || rest->is_smart
             ? WithBoneAim(character, pose, bone, target)
             : WithBoneReach(character, pose, bone, target, 3);
}

CharacterPose WithBoneShift(CharacterPose pose, BoneId bone, QPointF by) {
  assert(std::isfinite(by.x()));
  assert(bone.value() >= 0);
  pose.bones[bone].offset += by;
  return pose;
}

CharacterPose WithBoneScale(CharacterPose pose, BoneId bone, double scale) {
  assert(std::isfinite(scale));
  assert(bone.value() >= 0);
  pose.bones[bone].scale = std::clamp(scale, 0.01, 100.0);
  return pose;
}

CharacterPose WithPartShift(CharacterPose pose, PartId part, QPointF by) {
  assert(std::isfinite(by.x()));
  assert(part.value() >= 0);
  PartPose& own = part.IsValid() ? pose.parts[part] : pose.root;
  own.offset += by;
  return pose;
}

CharacterPose WithPartTurn(CharacterPose pose, PartId part, double degrees) {
  assert(std::isfinite(degrees));
  assert(part.value() >= 0);
  PartPose& own = part.IsValid() ? pose.parts[part] : pose.root;
  own.rotation += degrees;
  return pose;
}

CharacterPose WithPartScale(CharacterPose pose, PartId part, double scale_x,
                            double scale_y) {
  assert(std::isfinite(scale_x) && std::isfinite(scale_y));
  assert(part.value() >= 0);
  PartPose& own = part.IsValid() ? pose.parts[part] : pose.root;
  own.scale_x = std::clamp(scale_x, -100.0, 100.0);
  own.scale_y = std::clamp(scale_y, -100.0, 100.0);
  return pose;
}

CharacterPose WithPartOpacity(CharacterPose pose, PartId part,
                              double opacity) {
  assert(std::isfinite(opacity));
  assert(part.value() >= 0);
  PartPose& own = part.IsValid() ? pose.parts[part] : pose.root;
  own.opacity = std::clamp(opacity, 0.0, 1.0);
  return pose;
}

CharacterPose WithPointsShift(CharacterPose pose, PartId part,
                              std::span<const PointId> points, QPointF by) {
  assert(std::isfinite(by.x()));
  assert(part.IsValid());
  PartPose& own = pose.parts[part];
  for (PointId point : points) {
    own.points[point] += by;
  }
  return pose;
}

CharacterPose WithPick(CharacterPose pose, PartId switch_part, PartId child) {
  assert(switch_part.IsValid());
  assert(child.value() >= 0);
  pose.parts[switch_part].pick = child;
  return pose;
}

CharacterPose Trimmed(CharacterPose pose) {
  assert(pose.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(pose.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  for (auto& [id, part] : pose.parts) {
    std::erase_if(part.points, [](const auto& entry) {
      return QLineF(QPointF(), entry.second).length() < 1e-9;
    });
  }
  std::erase_if(pose.parts,
                [](const auto& entry) { return entry.second == PartPose{}; });
  std::erase_if(pose.bones,
                [](const auto& entry) { return entry.second == BonePose{}; });
  return pose;
}

}  // namespace snapper
