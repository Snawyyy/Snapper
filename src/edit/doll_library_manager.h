#ifndef SNAPPER_EDIT_DOLL_LIBRARY_MANAGER_H_
#define SNAPPER_EDIT_DOLL_LIBRARY_MANAGER_H_

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <map>

#include "base/error.h"
#include "io/doll_file.h"

namespace snapper {

class HistoryManager;

// The doll library on disk (folders the Krita exporter writes) and the
// dolls copied into the project from it.
class DollLibraryManager final : public QObject {
  Q_OBJECT

 public:
  DollLibraryManager(HistoryManager* history, QString library_folder);

  const QString& folder() const { return folder_; }
  // Names of the dolls in the library, sorted.
  QStringList Available() const;

  // Why Import can't act on name, for the greyed-out button; empty when
  // it can.
  QString WhyNoImport(const QString& name) const;
  // Copies a library doll into the project. The report says what the
  // rig had to change to fit the art.
  Result<ReconcileReport> Import(const QString& name);
  // Takes the newest art from the library for a project doll, keeping
  // its rig (matched by piece name).
  Result<ReconcileReport> Reload(const QString& name);
  // Writes the project's rig of a doll back to the library, so other
  // projects that import it get the same rig.
  Result<void> SaveRig(const QString& name);
  // Takes a doll out of the project; refused while a layer uses it.
  Result<void> Remove(const QString& name);
  // How many layers in the project show the doll.
  int UseCount(const QString& name) const;

 signals:
  // Dolls were added to or removed from the library folder.
  void LibraryChanged();
  // A project doll's art was re-exported from Krita; Reload takes it.
  void ArtChanged(const QString& name);

 private:
  QString FolderOf(const QString& name) const;
  QDateTime ArtStamp(const QString& name) const;
  void Rescan();

  HistoryManager* history_;
  QString folder_;
  QFileSystemWatcher watcher_;
  // When each project doll's art was last taken, to spot re-exports.
  std::map<QString, QDateTime> stamps_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DOLL_LIBRARY_MANAGER_H_
