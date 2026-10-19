#include "anim/bone_ik.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/character_pose.h"

namespace snapper {
namespace {

// Rounds of turning the chain; each round turns every bone once.
constexpr int kIkRounds = 12;
// Close enough, in pixels.
constexpr double kIkReached = 0.25;

const PosedBone* Posed(const std::vector<PosedBone>& bones, BoneId id) {
  assert(bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  assert(id.IsValid());
  const auto found = std::ranges::find_if(
      bones, [id](const PosedBone& bone) { return bone.id == id; });
  return found == bones.end() ? nullptr : &*found;
}

// Degrees from a to b, folded into -180 to 180.
double AngleBetween(QPointF from, QPointF a, QPointF b) {
  assert(std::isfinite(a.x()));
  assert(std::isfinite(b.x()));
  const double to_a = std::atan2(a.y() - from.y(), a.x() - from.x());
  const double to_b = std::atan2(b.y() - from.y(), b.x() - from.x());
  double turn = (to_b - to_a) * 180.0 / std::numbers::pi;
  turn = std::remainder(turn, 360.0);
  return turn;
}

}  // namespace

double LimitTurn(const Bone& bone, double turn) {
  assert(std::isfinite(turn));
  assert(bone.min_turn <= bone.max_turn || !bone.has_limits);
  return bone.has_limits ? std::clamp(turn, bone.min_turn, bone.max_turn)
                         : turn;
}

std::vector<BoneId> IkChainOf(const Character& character, BoneId bone,
                              int limit) {
  assert(limit >= 0);
  assert(limit <= kMaxCharacterBones);
  std::vector<BoneId> chain;
  const Bone* at = FindBone(character, bone);
  while (at != nullptr && static_cast<int>(chain.size()) < limit &&
         !at->is_locked && !at->is_smart) {
    chain.push_back(at->id);
    at = at->parent.IsValid() ? FindBone(character, at->parent) : nullptr;
  }
  return chain;
}

CharacterPose ReachFor(const Character& character, const CharacterPose& pose,
                       BoneId bone, QPointF target, int chain_limit) {
  assert(IsSound(character));
  assert(chain_limit >= 0);
  CharacterPose out = pose;
  const std::vector<BoneId> chain = IkChainOf(character, bone, chain_limit);
  for (int round = 0; round < kIkRounds && !chain.empty(); ++round) {
    for (BoneId link : chain) {
      const std::vector<PosedBone> bones = PoseBones(character, out);
      const PosedBone* end = Posed(bones, bone);
      const PosedBone* pivot = Posed(bones, link);
      assert(end != nullptr && pivot != nullptr);
      const double turn = AngleBetween(pivot->base, end->tip, target);
      const Bone* rest = FindBone(character, link);
      BonePose& own = out.bones[link];
      own.turn = LimitTurn(*rest, own.turn + turn);
    }
    const std::vector<PosedBone> bones = PoseBones(character, out);
    if (QLineF(Posed(bones, bone)->tip, target).length() < kIkReached) {
      break;
    }
  }
  return out;
}

}  // namespace snapper
