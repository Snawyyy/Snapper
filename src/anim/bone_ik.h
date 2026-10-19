#ifndef SNAPPER_ANIM_BONE_IK_H_
#define SNAPPER_ANIM_BONE_IK_H_

#include <QPointF>

#include <vector>

#include "model/character.h"

namespace snapper {

// The bones an IK drag of bone turns: bone and its parents up to the
// first locked one or the root, at most limit of them, bone first.
std::vector<BoneId> IkChainOf(const Character& character, BoneId bone,
                              int limit);

// pose with the chain of bone turned so bone's tip reaches toward target
// (character space): a few rounds of turning each bone in the chain,
// tip first, toward the target, within each bone's limits. Bones out of
// the chain keep their turn.
CharacterPose ReachFor(const Character& character, const CharacterPose& pose,
                       BoneId bone, QPointF target, int chain_limit);

// turn clamped to bone's limits, when it has them.
double LimitTurn(const Bone& bone, double turn);

}  // namespace snapper

#endif  // SNAPPER_ANIM_BONE_IK_H_
