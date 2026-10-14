#ifndef SNAPPER_ANIM_LAYER_ORDER_H_
#define SNAPPER_ANIM_LAYER_ORDER_H_

#include <vector>

#include "base/frame.h"
#include "model/shot.h"

namespace snapper {

// The layers of shot showing at local, back to front, as they are drawn
// and as a click finds them: the shot's stack, except that a layer
// whose own pose has draw order N at local is drawn N places in front
// (N above 0) or behind (N below 0) of its place, past any kind of
// layer, stopping at the front or back of the stack. Only layers
// showing count as places. Layers that land on the same place keep
// their stack order. Everything that draws or picks layers of a shot
// goes through here, so what is clicked is what is drawn.
std::vector<const Layer*> LayersInDrawOrder(const Shot& shot, Frame local);

}  // namespace snapper

#endif  // SNAPPER_ANIM_LAYER_ORDER_H_
