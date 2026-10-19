#ifndef SNAPPER_UI_CHARACTER_THUMB_H_
#define SNAPPER_UI_CHARACTER_THUMB_H_

#include <QPixmap>

#include "model/character.h"

namespace snapper {

// character at rest, fitted in a side by side square with a margin, on
// a clear ground: the picture its card shows.
QPixmap CharacterPicture(const Character& character, int side);

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_THUMB_H_
