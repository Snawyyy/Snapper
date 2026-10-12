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
  // Old dolls (a single doll.json from the first Snapper) that couldn't
  // be turned into today's files, and why.
  const QStringList& conversion_problems() const { return problems_; }
  // Names of the dolls in the library, sorted.
  QStringList Available() const;
  // The folder on disk of the library doll called name.
  QString FolderOf(const QString& name) const;

  // Why Import can't act on name, for the greyed-out button; empty when
  // it can.
  QString WhyNoImport(const QString& name) const;
  // Copies a library doll into the project. The report says what the
  // rig had to change to fit the art.
  Result<ReconcileReport> Import(const QString& name);
  // Takes the newest art from the library for a project doll, keeping
  // its rig (matched by piece name).
  Result<ReconcileReport> Reload(const QString& name);
  // Imports or reloads several dolls as one step; what each changed
  // in its rig, by name. Stops at the first that fails.
  Result<std::map<QString, ReconcileReport>> ImportAll(
      const QStringList& names);
  Result<std::map<QString, ReconcileReport>> ReloadAll(
      const QStringList& names);
  // Writes the project's rig of a doll back to the library, so other
  // projects that import it get the same rig.
  Result<void> SaveRig(const QString& name);
  // Takes a doll out of the project; refused while a layer uses it.
  Result<void> Remove(const QString& name);
  // How many layers in the project show the doll.
  int UseCount(const QString& name) const;

  // Doll to Krita. The Krita file the library doll called name was
  // last written to (kept in its rig.json), or empty when never.
  QString KritaFileOf(const QString& name) const;
  // Where a first Doll to Krita offers to save: beside the library.
  QString SuggestedKritaFile(const QString& name) const;
  // True when the doll's art or rig changed after its Krita file was
  // written, so opening it again should offer to rebuild it.
  bool IsKritaFileStale(const QString& name) const;
  // Writes the library doll called name as an OpenRaster file at path
  // (with the project's rig when the doll is in the project, for the
  // joint crosses) and remembers path in its rig.json.
  Result<void> ExportToKrita(const QString& name, const QString& path);
  // The Krita program: the one set in settings, else one on the PATH;
  // empty when none is found.
  QString KritaProgram() const;
  void SetKritaProgram(const QString& program);
  // Opens path in Krita. Refused, saying why, when Krita isn't found,
  // so the caller can show the file instead.
  Result<void> OpenInKrita(const QString& path) const;

 signals:
  // Dolls were added to or removed from the library folder.
  void LibraryChanged();
  // A project doll's art was re-exported from Krita; Reload takes it.
  void ArtChanged(const QString& name);

 private:
  QDateTime ArtStamp(const QString& name) const;
  QString SettingsPath() const;
  void Rescan();

  HistoryManager* history_;
  QString folder_;
  void ConvertOldDolls();

  QFileSystemWatcher watcher_;
  QStringList problems_;
  // When each project doll's art was last taken, to spot re-exports.
  std::map<QString, QDateTime> stamps_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DOLL_LIBRARY_MANAGER_H_
