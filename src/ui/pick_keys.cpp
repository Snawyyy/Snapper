#include "ui/pick_keys.h"

#include <cassert>

namespace snapper {

std::optional<PickMode> ClickMode(Qt::KeyboardModifiers mods,
                                  bool is_picked) {
  const bool is_ctrl = mods.testFlag(Qt::ControlModifier);
  const bool is_shift = mods.testFlag(Qt::ShiftModifier);
  const bool is_kept = !is_ctrl && !is_shift && is_picked;
  const std::optional<PickMode> mode =
      is_ctrl    ? std::optional(PickMode::kToggle)
      : is_shift ? std::optional(PickMode::kAdd)
      : is_kept  ? std::nullopt
                 : std::optional(PickMode::kReplace);
  assert(mode.has_value() || is_picked);
  assert(mode != PickMode::kRemove);
  return mode;
}

PickMode BoxMode(Qt::KeyboardModifiers mods) {
  const bool is_ctrl = mods.testFlag(Qt::ControlModifier);
  const bool is_shift = mods.testFlag(Qt::ShiftModifier);
  const PickMode mode = is_ctrl    ? PickMode::kRemove
                        : is_shift ? PickMode::kAdd
                                   : PickMode::kReplace;
  assert(mode != PickMode::kToggle);
  return mode;
}

}  // namespace snapper
