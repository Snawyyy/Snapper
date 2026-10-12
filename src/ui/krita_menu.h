#ifndef SNAPPER_UI_KRITA_MENU_H_
#define SNAPPER_UI_KRITA_MENU_H_

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QString>

namespace snapper {

class DollLibraryManager;
class ToastManager;

// Doll to Krita from the window: the File menu's "Export doll to Krita"
// (one entry per library doll) and "Krita program...", and Open, which
// the cast cards' right click calls too. The library decides where the
// file goes and whether it is stale; this only asks and shows.
class KritaMenu final : public QObject {
  Q_OBJECT

 public:
  KritaMenu(DollLibraryManager* library, ToastManager* toasts,
            QWidget* window);

  QMenu* menu() { return &menu_; }
  QAction* program_action() { return &program_action_; }

  // Opens the doll's Krita file in Krita: asks where to save it the
  // first time, offers to rebuild it when the doll changed since, and
  // shows the file in its folder when Krita isn't found.
  void Open(const QString& doll);

 private:
  void Fill();
  void ChooseProgram();
  // The file to open for doll, written if needed; empty when the user
  // cancelled or writing failed (and was reported).
  QString Prepare(const QString& doll);

  DollLibraryManager* library_;
  ToastManager* toasts_;
  QWidget* window_;
  QMenu menu_;
  QAction program_action_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KRITA_MENU_H_
