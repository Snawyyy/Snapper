#ifndef SNAPPER_UI_STORYBOARD_ACTIONS_H_
#define SNAPPER_UI_STORYBOARD_ACTIONS_H_

#include <QMenu>
#include <QString>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The ways into drawing a storyboard frame, all at the playhead of the
// shot under it (StoryboardManager). Draw mode itself is the one the
// pieces use.

// Why a storyboard frame can't be drawn at the playhead (is_over: over
// the frame showing, else a new one starting there); empty when it can.
QString WhyNoStoryboardHere(const Managers& managers, bool is_over);
// Starts draw mode on the storyboard at the playhead, or toasts why not.
void RequestStoryboard(const Managers& managers, bool is_over);
// Adds "New storyboard frame here" and "Draw over storyboard frame" to
// menu (the stage's right click), greyed with the reason when they
// can't act.
void AddStoryboardActions(QMenu* menu, const Managers& managers);
// Alt+N (a new blank frame at the playhead) and Alt+Shift+N (draw over
// the frame showing) while widget or a child of it has focus.
void AddStoryboardKeys(QWidget* widget, const Managers& managers);

}  // namespace snapper

#endif  // SNAPPER_UI_STORYBOARD_ACTIONS_H_
