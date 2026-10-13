#ifndef SNAPPER_UI_PICK_KEYS_H_
#define SNAPPER_UI_PICK_KEYS_H_

#include <Qt>

#include <optional>

#include "edit/selection_manager.h"

namespace snapper {

// The pick rule every list, canvas and strip shares: which modifier
// keys mean what. Only these two functions read the keys for a pick, so
// Shift+Ctrl means the same everywhere.

// How a click picks what it lands on.
// - Ctrl flips it in or out, and wins over Shift.
// - Shift adds it.
// - A plain click on something already picked keeps the whole pick
//   (nullopt), so the press can drag all of it; a release that did not
//   drag then narrows the pick to it (ClickMode(mods, false)).
// - A plain click on anything else picks only it.
std::optional<PickMode> ClickMode(Qt::KeyboardModifiers mods, bool is_picked);

// How a box dragged from empty space picks what it touches: Shift adds,
// Ctrl takes out (and wins over Shift), plain replaces.
PickMode BoxMode(Qt::KeyboardModifiers mods);

}  // namespace snapper

#endif  // SNAPPER_UI_PICK_KEYS_H_
