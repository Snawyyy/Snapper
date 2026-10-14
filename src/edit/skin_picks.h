#ifndef SNAPPER_EDIT_SKIN_PICKS_H_
#define SNAPPER_EDIT_SKIN_PICKS_H_

#include "base/error.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// project where the doll layer's pieces that a skin it wears at frame
// names lose their hand-picked drawing, from frame up to where it next
// wears other skins: putting a skin on wins over a drawing swapped by
// hand earlier, so a piece swapped to a blank drawing comes back.
// Pose keys there keep their pose; only the drawing goes back to the
// default.
Result<Project> WithSkinPicksCleared(Project project, ShotId shot,
                                     LayerId layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_EDIT_SKIN_PICKS_H_
