#ifndef SNAPPER_IO_CHARACTER_FILE_H_
#define SNAPPER_IO_CHARACTER_FILE_H_

#include <QString>
#include <QStringList>

#include "base/error.h"
#include "model/character.h"

namespace snapper {

// A library character is a folder holding character.json and the
// pictures its image parts use.
inline constexpr char kCharacterFile[] = "character.json";

// Reads the character in folder; its folder is set to folder.
Result<Character> ReadCharacterFolder(const QString& folder);
// Writes character.json into folder, making the folder if needed.
Result<void> WriteCharacterFolder(const QString& folder,
                                  const Character& character);
// The names of the character folders inside library, sorted.
QStringList CharacterFolders(const QString& library);

}  // namespace snapper

#endif  // SNAPPER_IO_CHARACTER_FILE_H_
