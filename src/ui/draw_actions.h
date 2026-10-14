#ifndef SNAPPER_UI_DRAW_ACTIONS_H_
#define SNAPPER_UI_DRAW_ACTIONS_H_

#include <QComboBox>
#include <QMenu>
#include <QString>
#include <QWidget>

#include "edit/draw_manager.h"
#include "ui/managers.h"

namespace snapper {

// The ways into draw mode, all on the picked piece at the playhead.

// Why draw mode can't start there now (with is_edit, on the drawing the
// piece shows); empty when it can.
QString WhyNoDrawHere(const Managers& managers, bool is_edit);
// Starts a new drawing from start, or toasts why it can't.
void RequestDraw(const Managers& managers, DrawStart start);
// Starts on the drawing the piece shows, or toasts why it can't.
void RequestEditDrawing(const Managers& managers);
// Adds the Drawing picker's "+" entries at the end of box: a new drawing
// blank, as a copy of the one showing, or of the default; greyed with
// the reason when none can be made. How many it added.
int AddDrawStarts(QComboBox* box, const Managers& managers);
// What the picker's entry at (an index among those AddDrawStarts added)
// starts from.
DrawStart DrawStartAt(int at);
// Adds "Draw new drawing" and "Edit drawing" to menu (the stage's right
// click), then the storyboard's two (ui/storyboard_actions.h), greyed
// with the reason when they can't act.
void AddDrawActions(QMenu* menu, const Managers& managers);
// N (new drawing, a copy of the one showing) and Shift+N (edit the one
// showing) while widget or a child of it has focus, and Alt+N and
// Alt+Shift+N for the storyboard (ui/storyboard_actions.h).
void AddDrawKeys(QWidget* widget, const Managers& managers);

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_ACTIONS_H_
