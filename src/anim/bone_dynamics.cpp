#include "anim/bone_dynamics.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <numbers>
#include <vector>

#include "anim/character_pose.h"

namespace snapper {
namespace {

struct Trail final {
  QPointF tip;
  QPointF speed;
};

// Spring pull and damping for a bone's dynamics (0 to 1): looser and
// longer ringing as it grows.
double PullOf(double dynamics) {
  assert(dynamics > 0.0 && dynamics <= 1.0);
  return 0.55 - 0.45 * dynamics;
}

double DampOf(double dynamics) {
  assert(dynamics > 0.0 && dynamics <= 1.0);
  return 0.45 - 0.3 * dynamics;
}

// Bouncing bones, parents before children.
std::vector<const Bone*> Bouncers(const Character& character) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  std::vector<std::pair<int, const Bone*>> found;
  for (const CharacterPart& part : character.parts) {
    for (const Bone& bone : part.bones) {
      if (bone.dynamics <= 0.0 || bone.is_smart) {
        continue;
      }
      int depth = 0;
      for (const Bone* up = &bone; up->parent.IsValid() && depth < kMaxCharacterBones;
           ++depth) {
        up = FindBone(character, up->parent);
        if (up == nullptr) break;
      }
      found.emplace_back(depth, &bone);
    }
  }
  std::ranges::stable_sort(found, {}, &std::pair<int, const Bone*>::first);
  std::vector<const Bone*> bones;
  for (const auto& [depth, bone] : found) bones.push_back(bone);
  return bones;
}

const PosedBone* Find(const std::vector<PosedBone>& bones, BoneId id) {
  assert(id.IsValid());
  assert(bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  const auto found = std::ranges::find_if(
      bones, [id](const PosedBone& bone) { return bone.id == id; });
  return found == bones.end() ? nullptr : &*found;
}

}  // namespace

bool HasDynamics(const Character& character) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(character.next_id >= 1);
  return std::ranges::any_of(character.parts, [](const CharacterPart& part) {
    return std::ranges::any_of(part.bones, [](const Bone& bone) {
      return bone.dynamics > 0.0 && !bone.is_smart;
    });
  });
}

CharacterPose WithDynamics(
    const Character& character, Frame frame,
    const std::function<CharacterPose(Frame)>& pose_at) {
  assert(frame.index() >= 0);
  assert(pose_at != nullptr);
  const std::vector<const Bone*> bouncers = Bouncers(character);
  if (bouncers.empty()) {
    return pose_at(frame);
  }
  std::map<BoneId, Trail> trails;
  CharacterPose out;
  const int first = std::max(0, frame.index() - kDynamicsWindow);
  for (int at = first; at <= frame.index(); ++at) {
    out = pose_at(Frame(at));
    // Each bouncer in turn, on top of the bouncers before it.
    for (const Bone* bone : bouncers) {
      const std::vector<PosedBone> posed = PoseBones(character, out);
      const PosedBone* now = Find(posed, bone->id);
      if (now == nullptr) continue;
      Trail& trail = trails.try_emplace(bone->id, Trail{now->tip, {}}).first->second;
      const QPointF pull = (now->tip - trail.tip) * PullOf(bone->dynamics);
      trail.speed = trail.speed * (1.0 - DampOf(bone->dynamics)) + pull;
      trail.tip += trail.speed;
      const double wanted = std::atan2(trail.tip.y() - now->base.y(),
                                       trail.tip.x() - now->base.x());
      const double held = std::atan2(now->tip.y() - now->base.y(),
                                     now->tip.x() - now->base.x());
      const double lag =
          std::remainder((wanted - held) * 180.0 / std::numbers::pi, 360.0);
      out.bones[bone->id].turn += std::clamp(lag, -120.0, 120.0);
    }
  }
  return out;
}

}  // namespace snapper
