#ifndef SNAPPER_UI_TOOL_ICONS_H_
#define SNAPPER_UI_TOOL_ICONS_H_

#include <QIcon>

#include "ui/character_session.h"

namespace snapper {

// A small drawn mark for each Character tab tool, for the tool box down
// the left of the canvas: an arrow for Pick, a pen nib for Add point, a
// horseshoe for Magnet...
QIcon ToolIcon(CharacterTool tool);

}  // namespace snapper

#endif  // SNAPPER_UI_TOOL_ICONS_H_
