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
// click), greyed with the reason when they can't act.
void AddDrawActions(QMenu* menu, const Managers& managers);
// N (new drawing, a copy of the one showing) and Shift+N (edit the one
// showing) while widget or a child of it has focus; Alt+N and
// Alt+Shift+N do the same for the shot's draft.
void AddDrawKeys(QWidget* widget, const Managers& managers);

// The shot's draft at the playhead (DraftManager).

// Why a draft cel can't be drawn at the playhead (is_over: over the cel
// showing, else a new one starting there); empty when it can.
QString WhyNoDraftHere(const Managers& managers, bool is_over);
// Starts draw mode on the draft at the playhead, or toasts why not.
void RequestDraft(const Managers& managers, bool is_over);

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_ACTIONS_H_
