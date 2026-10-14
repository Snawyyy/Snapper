#include "model/layer.h"

#include <cassert>
#include <variant>

namespace snapper {

bool IsLayerLive(const Layer& layer, Frame frame, Frame shot_length) {
  assert(frame.index() >= 0);
  assert(shot_length.index() >= 0);
  const bool is_in_shot = frame < shot_length;
  const bool is_started = !(frame < layer.start);
  const bool is_to_end = layer.length.index() == 0;
  const bool is_before_end =
      is_to_end || frame.index() < layer.start.index() + layer.length.index();
  return layer.is_visible && is_in_shot && is_started && is_before_end;
}

bool IsStagePicture(const Layer& layer) {
  assert(layer.id.IsValid());
  assert(layer.length.index() >= 0);
  const bool is_effect = std::holds_alternative<EffectLayer>(layer.content);
  const bool is_board = std::holds_alternative<StoryboardLayer>(layer.content);
  return !is_effect && !is_board;
}

}  // namespace snapper
