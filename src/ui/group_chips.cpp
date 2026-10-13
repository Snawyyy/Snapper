#include "ui/group_chips.h"

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "ui/pick_keys.h"

namespace snapper {

ChipLook ChipOf(const ShownGroup& group, const std::vector<WarpDot>& picked) {
  assert(!group.name.isEmpty());
  assert(picked.size() < 10000000);
  const auto in_group = std::ranges::count_if(
      picked,
      [&group](const WarpDot& dot) { return group.dots.contains(dot); });
  const bool is_all = !group.dots.empty() &&
                      static_cast<size_t>(in_group) == group.dots.size();
  const bool is_some = in_group > 0;
  const GroupShare share = is_all    ? GroupShare::kAll
                           : is_some ? GroupShare::kSome
                                     : GroupShare::kNone;
  QString tip = Tr("%1 dots, %2 picked. Click picks them, Shift adds, "
                   "Ctrl flips. Right click for more.")
                    .arg(group.dots.size())
                    .arg(in_group);
  const bool is_default = group.is_default;
  if (is_default) {
    tip += Tr(" A default: every shot with this doll shows it.");
  }
  return {Tr("%1 (%2)").arg(group.name).arg(group.dots.size()), tip, share};
}

std::vector<ShownGroup> GroupsOfPicks(const std::vector<ShownGroup>& groups,
                                      const std::set<Pick>& picks) {
  assert(groups.size() < 1000);
  assert(picks.size() < 1000000);
  std::vector<ShownGroup> shown;
  for (const ShownGroup& group : groups) {
    const bool is_picked = std::any_of(
        group.dots.begin(), group.dots.end(), [&picks](const WarpDot& dot) {
          return picks.contains({dot.layer, dot.piece}) ||
                 picks.contains({dot.layer, QString()});
        });
    if (is_picked) {
      shown.push_back(group);
    }
  }
  return shown;
}

PickMode GroupPickMode(Qt::KeyboardModifiers keys) {
  // A chip is never dragged, so a plain click always picks only it.
  const std::optional<PickMode> mode = ClickMode(keys, false);
  assert(mode.has_value());
  assert(PickMode::kReplace != PickMode::kAdd);
  return mode.value_or(PickMode::kReplace);
}

PickMode GroupPickMode(bool is_shift, bool is_ctrl) {
  Qt::KeyboardModifiers keys;
  keys.setFlag(Qt::ShiftModifier, is_shift);
  keys.setFlag(Qt::ControlModifier, is_ctrl);
  return GroupPickMode(keys);
}

}  // namespace snapper
