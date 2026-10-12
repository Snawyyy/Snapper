#ifndef SNAPPER_UI_HOLD_ACTIONS_H_
#define SNAPPER_UI_HOLD_ACTIONS_H_

#include <QMenu>

#include <optional>

#include "base/frame.h"
#include "model/shot.h"
#include "ui/managers.h"

namespace snapper {

// The stage's right click entries for picked objects at frame of shot:
// "Hold cup with Mia's hand" when hit is a doll piece (greyed with the
// reason when it can't), "Drop cup here" while one is held, and "Remove
// hold change" on a frame where one changes. Nothing without picked
// objects or the object manager.
void AddHoldActions(QMenu* menu, const Managers& managers, ShotId shot,
                    Frame frame, const std::optional<LinkEnd>& hit);

}  // namespace snapper

#endif  // SNAPPER_UI_HOLD_ACTIONS_H_
