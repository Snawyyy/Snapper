#ifndef SNAPPER_EDIT_DOCUMENT_MANAGER_H_
#define SNAPPER_EDIT_DOCUMENT_MANAGER_H_

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "base/error.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;

constexpr int kMaxRecentFiles = 10;
// Two minutes: little enough lost to a crash, rare enough to not stall.
constexpr int kAutosaveMs = 2 * 60 * 1000;

// The project as a file: new, open, save, the settings that belong to
// the whole document, autosave and the recent files list.
class DocumentManager final : public QObject {
  Q_OBJECT

 public:
  // data_folder holds the autosave and the recent files list.
  DocumentManager(HistoryManager* history, QString data_folder);

  Result<void> New(const QString& name, CanvasSize canvas);
  Result<void> Open(const QString& path);
  // Saves where the project was opened or last saved. Fails, saying so,
  // when it has never been saved: the caller asks where (SaveAs).
  Result<void> Save();
  Result<void> SaveAs(const QString& path);

  const QString& path() const { return path_; }
  bool HasPath() const { return !path_.isEmpty(); }
  QStringList RecentFiles() const;

  // Whole-document settings, each one undoable step.
  Result<void> Rename(const QString& name);
  Result<void> SetCanvas(CanvasSize canvas);
  // Empty clears the song.
  Result<void> SetSong(const QString& song);

  // Writes the project to the autosave file if it has unsaved changes.
  Result<void> Autosave();
  QString AutosavePath() const;
  // True when an autosave is left over from a session that didn't end
  // cleanly.
  bool HasAutosave() const;
  // Opens the leftover autosave as an unsaved project.
  Result<void> Recover();
  // Removes the autosave; for a clean exit or a declined recovery.
  void DiscardAutosave();

 signals:
  void PathChanged();
  void RecentFilesChanged();
  // The timed autosave couldn't write; the UI tells the user why.
  void AutosaveFailed(const QString& why);

 private:
  void Remember(const QString& path);
  QString SettingsPath() const;

  HistoryManager* history_;
  QString data_folder_;
  QString path_;
  QTimer autosave_timer_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DOCUMENT_MANAGER_H_
