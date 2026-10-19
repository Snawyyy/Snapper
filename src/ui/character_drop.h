#ifndef SNAPPER_UI_CHARACTER_DROP_H_
#define SNAPPER_UI_CHARACTER_DROP_H_

#include <QString>
#include <QStringList>

#include "ui/managers.h"

namespace snapper {

// Puts library characters on the picked shot's stage, taking into the
// project first any that are not in it, and picks the new layers.
// Returns what went wrong in plain words, empty when it worked; with no
// shot it says to add one.
QString PutCharactersOnStage(const Managers& managers,
                             const QStringList& characters);

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_DROP_H_
