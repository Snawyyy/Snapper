#ifndef SNAPPER_ANIM_IK_H_
#define SNAPPER_ANIM_IK_H_

#include <QPointF>

#include <optional>

#include "anim/doll_pose.h"
#include "model/doll.h"

namespace snapper {

// New rotations (degrees, as stored in the poses) for a chain's two
// pieces, and whether the tip actually reaches the target.
struct IkSolution final {
  double upper_rotation = 0.0;
  double lower_rotation = 0.0;
  bool is_reached = false;
};

// Bends chain so its tip lands on target (doll space), or points at it
// as far as the bones reach. Nothing when the chain names pieces the
// doll lacks, or a bone has no length.
std::optional<IkSolution> SolveIk(const Doll& doll, const PoseMap& poses,
                                  const IkChain& chain, QPointF target);

}  // namespace snapper

#endif  // SNAPPER_ANIM_IK_H_
