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

  // is_transparent: the Transparent start, whose shots start
  // see-through.
  Result<void> New(const QString& name, CanvasSize canvas,
                   bool is_transparent = false);
  Result<void> Open(const QString& path);
  // Saves where the project was opened or last saved. Fails, saying so,
  // when it has never been saved: the caller asks where (SaveAs).
  Result<void> Save();
  // Saving under a new name also copies the storyboard frames' folder
  // beside the new file and points the project at the copies, so the
  // frames follow the project.
  Result<void> SaveAs(const QString& path);

  const QString& path() const { return path_; }
  bool HasPath() const { return !path_.isEmpty(); }
  QStringList RecentFiles() const;
  // Where Save as should start: the file the project is in, else the
  // folder last opened or saved to, with the project's name.
  QString SuggestedSavePath() const;

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
  // Opens the leftover autosave as an unsaved project, which goes back
  // to the file it was being worked on in (Save writes there), or to no
  // file when it had never been saved.
  Result<void> Recover();
  // Removes the autosave; for a clean exit or a declined recovery.
  void DiscardAutosave();
  // The file the app's settings live in, shared by whoever saves one.
  QString SettingsPath() const;

  // Whether the stage draws things past the frame edge. Remembered for
  // each project file, not kept in it nor undoable; a new project
  // starts with it on.
  bool IsOutsideShown() const { return is_outside_shown_; }
  void SetOutsideShown(bool is_shown);

 signals:
  void PathChanged();
  void RecentFilesChanged();
  // The timed autosave couldn't write; the UI tells the user why.
  void AutosaveFailed(const QString& why);
  void OutsideShownChanged();

 private:
  // The storyboard frames of the project as it is now go beside target.
  Result<void> CarryStoryboard(const QString& target);
  void Remember(const QString& path);
  // Reads the outside switch for the file now open.
  void LoadOutsideShown();
  // Writes it for the file now open, when there is one.
  void StoreOutsideShown() const;

  HistoryManager* history_;
  QString data_folder_;
  QString path_;
  bool is_outside_shown_ = true;
  QTimer autosave_timer_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DOCUMENT_MANAGER_H_
