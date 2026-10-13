#ifndef SNAPPER_UI_LIBRARY_FOLDER_PROMPTS_H_
#define SNAPPER_UI_LIBRARY_FOLDER_PROMPTS_H_

#include <QString>

class QWidget;

namespace snapper {

class LibraryFolderManager;

// Asks for a name and makes a folder in the folder at in. Returns why it
// could not, or nothing when it was made or the asking was cancelled.
QString AskNewFolder(QWidget* parent, LibraryFolderManager* folders,
                     const QString& in);

// Asks for a new name for the folder at path and renames it; the same
// return.
QString AskRenameFolder(QWidget* parent, LibraryFolderManager* folders,
                        const QString& path);

}  // namespace snapper

#endif  // SNAPPER_UI_LIBRARY_FOLDER_PROMPTS_H_
