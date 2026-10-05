#ifndef SNAPPER_UI_MAIN_WINDOW_H_
#define SNAPPER_UI_MAIN_WINDOW_H_

#include <QAction>
#include <QMainWindow>
#include <QMenu>

namespace snapper {

class HistoryManager;

// The window frame: menus and, as they arrive, the panels. It shows
// manager state and forwards input; it decides nothing itself.
class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(HistoryManager* history);

 private:
  void BuildMenus();
  // Pulls title, undo and redo state from the history.
  void Refresh();

  HistoryManager* history_;
  QMenu file_menu_;
  QMenu edit_menu_;
  QAction quit_action_;
  QAction undo_action_;
  QAction redo_action_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MAIN_WINDOW_H_
