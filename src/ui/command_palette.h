#ifndef SNAPPER_UI_COMMAND_PALETTE_H_
#define SNAPPER_UI_COMMAND_PALETTE_H_

#include <QAction>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QString>
#include <QVBoxLayout>

#include <vector>

namespace snapper {

// One thing the menus can do, named as "Edit: Undo Rotate head".
struct Command final {
  QString name;
  QAction* action = nullptr;
};

// Every action in the menu bar, submenus included, with its menu in the
// name; separators and entries that only open a submenu are left out.
std::vector<Command> CommandsOf(const QMenuBar* bar);

// "Find a command": a box that drops over the window, searching every
// menu action by name (see MatchCommands) with its key beside it.
// Enter runs the one picked, a greyed one says why instead, and Escape
// closes it.
class CommandPalette final : public QDialog {
  Q_OBJECT

 public:
  explicit CommandPalette(QMenuBar* bar, QWidget* parent);

  // Shows the box over the window with a clean query.
  void Open();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void Search();
  void Run();

  QMenuBar* bar_;
  QVBoxLayout layout_;
  QLineEdit query_;
  QListWidget found_;
  QLabel* why_ = nullptr;
  // Every command, and the ones the query finds, in the order shown.
  std::vector<Command> commands_;
  std::vector<Command> shown_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMMAND_PALETTE_H_
