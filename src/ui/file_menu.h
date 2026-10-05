#ifndef SNAPPER_UI_FILE_MENU_H_
#define SNAPPER_UI_FILE_MENU_H_

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QString>

#include "base/error.h"

namespace snapper {

class DocumentManager;
class HistoryManager;

// The File menu and the questions around losing work: save before
// closing, recover after a crash. Dialogs open over window.
class FileMenu final : public QObject {
  Q_OBJECT

 public:
  FileMenu(DocumentManager* document, HistoryManager* history,
           QWidget* window);

  QMenu* menu() { return &menu_; }

  // Asks to save unsaved work. False when the user cancels, so whatever
  // was about to replace or close the project must stop.
  bool ConfirmDiscard();
  // On start: offers to recover work left by a crash.
  void OfferRecovery();

 private:
  void New();
  void Open();
  void OpenPath(const QString& path);
  bool Save();
  bool SaveAs();
  void ChooseSong();
  void RemoveSong();
  void RefreshRecent();
  void Refresh();
  // Tells the user why something failed; quiet when it worked.
  void Report(const Result<void>& result) const;

  DocumentManager* document_;
  HistoryManager* history_;
  QWidget* window_;
  QMenu menu_;
  QMenu recent_menu_;
  QAction new_action_;
  QAction open_action_;
  QAction save_action_;
  QAction save_as_action_;
  QAction song_action_;
  QAction remove_song_action_;
  QAction quit_action_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FILE_MENU_H_
