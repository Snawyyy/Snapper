#ifndef SNAPPER_ANIM_DOLL_BEAT_H_
#define SNAPPER_ANIM_DOLL_BEAT_H_

#include "base/frame.h"
#include "model/layer.h"

namespace snapper {

// The frame whose drag frame shows: drags move on the doll's own beat,
// so a doll on 3s wobbles on 3s. Each key is a beat, every frame of an
// ease is, and a long hold keeps beating at the spacing of the keys
// before it.
Frame BeatOf(const Layer& layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_BEAT_H_
