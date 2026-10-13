#ifndef SNAPPER_UI_GROUP_CHIPS_H_
#define SNAPPER_UI_GROUP_CHIPS_H_

#include <QString>
#include <Qt>

#include <set>
#include <vector>

#include "edit/group_edits.h"
#include "edit/selection_manager.h"

namespace snapper {

// How much of a group is picked now.
enum class GroupShare { kNone, kSome, kAll };

// What a group's chip says: its label, its tooltip and how much of it is
// picked (kAll shows the chip pressed).
struct ChipLook final {
  QString text;
  QString tip;
  GroupShare share = GroupShare::kNone;
};

// The chip of group while picked dots are picked.
ChipLook ChipOf(const ShownGroup& group, const std::vector<WarpDot>& picked);

// The groups that have a dot on a picked piece (a whole layer picked
// counts for all its pieces): a limb shows its own groups, not those of
// the limbs that are not picked.
std::vector<ShownGroup> GroupsOfPicks(const std::vector<ShownGroup>& groups,
                                      const std::set<Pick>& picks);

// How a click on a chip picks, from the keys held: the shared pick rule
// (ClickMode in pick_keys.h) for something not picked: Ctrl flips and
// wins over Shift, Shift adds, neither replaces.
PickMode GroupPickMode(Qt::KeyboardModifiers keys);
// The same from the two keys alone.
PickMode GroupPickMode(bool is_shift, bool is_ctrl);

}  // namespace snapper

#endif  // SNAPPER_UI_GROUP_CHIPS_H_
