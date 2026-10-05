#include "model/project.h"

#include <cassert>

namespace snapper {

bool IsValidCanvas(CanvasSize canvas) {
  static_assert(kMinCanvasSide > 0);
  static_assert(kMinCanvasSide <= kMaxCanvasSide);
  const auto is_valid_side = [](int side) {
    return side >= kMinCanvasSide && side <= kMaxCanvasSide;
  };
  // Video encoders need even sides.
  const bool is_even = canvas.width % 2 == 0 && canvas.height % 2 == 0;
  return is_valid_side(canvas.width) && is_valid_side(canvas.height) &&
         is_even;
}

}  // namespace snapper
