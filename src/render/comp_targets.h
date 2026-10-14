#ifndef SNAPPER_RENDER_COMP_TARGETS_H_
#define SNAPPER_RENDER_COMP_TARGETS_H_

#include "model/comp.h"
#include "render/comp.h"

namespace snapper {

// What a Comp effect works on, for render/comp.cpp; NeedsOf and
// CompSpace (render/comp.h) live beside these.

// How far (RGB distance) a pixel may sit from a picked target color and
// still count as it: the color key of the target and the plugins that
// match colors exactly (render/comp_color_snap.h) share it.
inline constexpr double kCompColorReach = 24.0;

// True when the effect targets layers (or pieces of them).
bool HasCompLayer(const CompEffect& effect);
// True for kinds that read the target's outline.
bool NeedsCompShape(const CompEffect& effect);
// The key of the effect's target picture; it must have a layer.
CompTargetKey CompKeyOf(const CompEffect& effect);
// The part of key that is layer, or nullptr when layer is not in it.
const CompTargetPart* PartOf(const CompTargetKey& key, LayerId layer);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_TARGETS_H_
