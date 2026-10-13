#include "ui/library_folder_prompts.h"

#include <QInputDialog>
#include <QLineEdit>

#include <cassert>

#include "edit/library_folder_manager.h"

namespace snapper {

QString AskNewFolder(QWidget* parent, LibraryFolderManager* folders,
                     const QString& in) {
  assert(folders != nullptr);
  assert(folders->Exists(in));
  bool is_ok = false;
  const QString name = QInputDialog::getText(
      parent, QObject::tr("New folder"),
      QObject::tr("Name of the folder in %1")
          .arg(LibraryFolderManager::AddressOf(in)),
      QLineEdit::Normal, QString(), &is_ok);
  if (!is_ok) {
    return QString();
  }
  const auto made = folders->AddFolder(in, name);
  return made ? QString() : made.error().message;
}

QString AskRenameFolder(QWidget* parent, LibraryFolderManager* folders,
                        const QString& path) {
  assert(folders != nullptr);
  assert(folders->Exists(path));
  bool is_ok = false;
  const QString name = QInputDialog::getText(
      parent, QObject::tr("Rename folder"),
      QObject::tr("New name for %1")
          .arg(LibraryFolderManager::AddressOf(path)),
      QLineEdit::Normal, path.mid(static_cast<int>(path.lastIndexOf('/')) + 1),
      &is_ok);
  if (!is_ok) {
    return QString();
  }
  const auto done = folders->RenameFolder(path, name);
  return done ? QString() : done.error().message;
}

}  // namespace snapper
