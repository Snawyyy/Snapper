#ifndef SNAPPER_UI_PART_ICONS_H_
#define SNAPPER_UI_PART_ICONS_H_

#include <QIcon>
#include <QString>

#include "model/character.h"

namespace snapper {

// A small drawn mark for each kind of character part: a pen curve for a
// drawing, a bone, a folder for a group, stacked cards for a switch, a
// landscape for a picture.
QIcon PartIcon(PartKind kind);
// The kind's name as the user reads it ("Drawing", "Bones"...).
QString PartKindName(PartKind kind);

}  // namespace snapper

#endif  // SNAPPER_UI_PART_ICONS_H_
