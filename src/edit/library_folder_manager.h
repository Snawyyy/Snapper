#ifndef SNAPPER_EDIT_LIBRARY_FOLDER_MANAGER_H_
#define SNAPPER_EDIT_LIBRARY_FOLDER_MANAGER_H_

#include <QObject>
#include <QString>
#include <QStringList>

#include <map>
#include <set>
#include <vector>

#include "base/error.h"

namespace snapper {

// Folders for the doll library, like folders on a computer: a folder holds
// dolls and other folders, one is open at a time, and Back, Forward and Up
// walk between the ones visited. A folder can be hidden; its dolls then
// leave the library list until Show hidden folders is on. Folders are
// only a way to sort the library: they live in a file of their own
// outside the project, so nothing here is an undoable edit, and a doll
// in a hidden folder stays usable wherever the project already has it.
//
// A folder is named by its path from the top: "Hair" or "Hair/Long"; the
// top itself is the empty path.
class LibraryFolderManager final : public QObject {
  Q_OBJECT

 public:
  explicit LibraryFolderManager(QString file_path);

  // The folder that is open, and how it reads in the address bar
  // ("Library/Hair/Long").
  const QString& current() const { return current_; }
  static QString AddressOf(const QString& path);
  // Opens the folder an address names; refused when there is none, or it
  // is hidden and hidden folders are not shown.
  Result<void> OpenAddress(const QString& address);
  Result<void> Open(const QString& path);
  QString WhyNoBack() const;
  QString WhyNoForward() const;
  QString WhyNoUp() const;
  void Back();
  void Forward();
  void Up();

  // The names of the folders directly in path that can be seen, sorted.
  QStringList FoldersIn(const QString& path) const;
  // Of all the dolls, the ones directly in path.
  QStringList DollsIn(const QString& path, const QStringList& all) const;
  // Every folder, as paths, sorted: for a Move to menu.
  QStringList AllFolders() const;
  bool Exists(const QString& path) const;
  QString FolderOf(const QString& doll) const;

  // Whether path itself is hidden, and whether it cannot be seen now
  // (it or a folder above it is hidden, and hidden ones are not shown).
  bool IsHidden(const QString& path) const;
  bool IsConcealed(const QString& path) const;
  bool show_hidden() const { return show_hidden_; }
  void SetShowHidden(bool is_shown);
  Result<void> SetHidden(const QString& path, bool is_hidden);

  // Makes a folder named name in parent (the path of the new folder).
  Result<QString> AddFolder(const QString& parent, const QString& name);
  Result<void> RenameFolder(const QString& path, const QString& name);
  // Removes a folder: what it holds moves up into its parent. Refused
  // when the parent already has a folder of the same name as one inside.
  Result<void> RemoveFolder(const QString& path);
  Result<void> MoveDolls(const QStringList& dolls, const QString& folder);

  // Why the file could not be read; empty when it could. While set,
  // nothing is saved, so a damaged file is never overwritten.
  const QString& load_error() const { return load_error_; }

 signals:
  void Changed();

 private:
  Result<void> CheckName(const QString& parent, const QString& name) const;
  // Applies change to the state; saves and tells everyone.
  void Settled();
  void Save() const;
  void Load();
  // The nearest folder from path up that can be seen.
  QString NearestSeen(const QString& path) const;

  QString file_path_;
  std::set<QString> folders_;
  std::map<QString, QString> doll_folder_;
  std::set<QString> hidden_;
  bool show_hidden_ = false;
  QString current_;
  std::vector<QString> back_;
  std::vector<QString> forward_;
  QString load_error_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_LIBRARY_FOLDER_MANAGER_H_
