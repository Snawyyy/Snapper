#ifndef SNAPPER_RENDER_STAGE_GEOMETRY_H_
#define SNAPPER_RENDER_STAGE_GEOMETRY_H_

#include <QTransform>

#include <optional>
#include <vector>

#include "anim/character_pose.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where things sit on a frame rendered at scale, for tools that draw
// handles and turn mouse moves into poses. "Screen" is output pixels.

// A layer's space to screen.
std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale);

// The bones of a character layer as it shows at local, in screen
// pixels; empty for any other layer.
std::vector<PosedBone> CharacterBonesOnScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              Frame local, double scale);

// The space a layer's own move is keyed in, to screen: the shot's.
QTransform LayerParentToScreen(const Project& project, const Shot& shot,
                               const Layer& layer, Frame local, double scale);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_GEOMETRY_H_
