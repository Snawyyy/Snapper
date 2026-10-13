#ifndef SNAPPER_RENDER_COMP_TARGETS_H_
#define SNAPPER_RENDER_COMP_TARGETS_H_

#include "model/comp.h"
#include "render/comp.h"

namespace snapper {

// What a Comp effect works on, for render/comp.cpp; NeedsOf and
// CompSpace (render/comp.h) live beside these.

// True when the effect targets a layer (or pieces of one).
bool HasCompLayer(const CompEffect& effect);
// True for kinds that read the target's outline.
bool NeedsCompShape(const CompEffect& effect);
// The key of the effect's target picture; it must have a layer.
CompTargetKey CompKeyOf(const CompEffect& effect);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_TARGETS_H_
