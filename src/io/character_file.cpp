#include "io/character_file.h"

#include <QDir>
#include <QFileInfo>

#include <cassert>

#include "base/text.h"
#include "io/character_json.h"
#include "io/json_file.h"

namespace snapper {

Result<Character> ReadCharacterFolder(const QString& folder) {
  assert(!folder.isEmpty());
  assert(folder.size() < 100000);
  const Result<QJsonObject> read =
      ReadJsonFile(QDir(folder).filePath(QLatin1String(kCharacterFile)));
  if (!read) {
    return std::unexpected(read.error());
  }
  JsonIssues issues;
  Character character = CharacterFromJson(*read, &issues);
  if (issues.HasIssue() && character.parts.empty()) {
    return std::unexpected(
        Error{Tr("%1 could not be read: %2").arg(folder, issues.first())});
  }
  if (character.name.isEmpty()) {
    character.name = QFileInfo(folder).fileName();
  }
  character.folder = folder;
  return character;
}

Result<void> WriteCharacterFolder(const QString& folder,
                                  const Character& character) {
  assert(!folder.isEmpty());
  assert(IsSound(character));
  const bool is_made = QDir().mkpath(folder);
  if (!is_made) {
    return std::unexpected(Error{Tr("Could not make the folder %1.").arg(folder)});
  }
  return WriteJsonFile(QDir(folder).filePath(QLatin1String(kCharacterFile)),
                       CharacterToJson(character));
}

QStringList CharacterFolders(const QString& library) {
  assert(!library.isEmpty());
  assert(library.size() < 100000);
  QStringList names;
  const QDir dir(library);
  for (const QString& name :
       dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
    if (QFileInfo::exists(dir.filePath(name + QLatin1Char('/') +
                                       QLatin1String(kCharacterFile)))) {
      names.push_back(name);
    }
  }
  return names;
}

}  // namespace snapper
