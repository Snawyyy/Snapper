#ifndef SNAPPER_UI_CAST_PANEL_H_
#define SNAPPER_UI_CAST_PANEL_H_

#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPixmap>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/doll_cards.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

class FoldSection;
class LibraryNavBar;

// Who is in the picked shot, in three plated sections (panel_kit.h).
// Library: import a doll, reload it after a Krita re-export (marked "new
// art"), or put it on the stage. Objects: props made from pictures, and
// Background, a picture as the bottom layer filling the canvas. Layers
// (folded at first; the timeline rows show the same): the shot's layers,
// top first: pick, tick to show or hide, add pictures, text and effects,
// copy, delete and restack. Every list picks by the one pick rule
// (PickList); Delete on cards takes unused dolls out of the project, on
// layers deletes them.
class CastPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit CastPanel(const Managers& managers);

 signals:
  void Problem(const QString& why);
  // The user asked to open a library doll in Krita (right click a card).
  void OpenInKritaRequested(const QString& doll);
  // A plain click on a library card: open that doll in the Rig tab's
  // panel (importing it into the project first when it is not in).
  void DollOpenAsked(const QString& doll);

 private:
  void BuildLayout();
  void Wire();
  void RefreshLibrary();
  // Rebuilds cards with a card per name, keeping the picked ones picked.
  void FillCards(DollCards* cards, const QStringList& names);
  // The right click menu of the library card under at (ui/cast_folders.cpp).
  void ShowLibraryMenu(const QPoint& at);
  // Library folders (ui/cast_folders.cpp): folder cards over the dolls of
  // the open folder, what a click, a menu or a Delete does to them.
  void AddFolderCards();
  QStringList PickedFolders() const;
  QStringList PickedLibraryDolls() const;
  void OpenFolderCard(const QListWidgetItem* item);
  void MoveDollsToFolder(const QStringList& dolls, const QString& folder);
  void DeleteInLibrary();
  void ShowFolderMenu(const QListWidgetItem* item, QMenu* menu);
  void AddMoveMenu(const QStringList& dolls, QMenu* menu);
  // The Objects row: pictures dropped or chosen become library objects;
  // a card's right click menu shows its folder or takes it out of the
  // project.
  void AddObjects(const QStringList& paths);
  void ChooseObjects();
  void ShowObjectMenu(const QPoint& at);
  // The small picture of a library doll, kept once drawn.
  QPixmap PictureOf(const QString& name);
  void RefreshLayers();
  void RefreshButtons();
  void Import();
  void Reload();
  // A plain click on a library card opens its doll in the rig.
  void OpenClicked(const QListWidgetItem* item);
  void Place();
  void AddPicture();
  void AddBackground();
  void AddText();
  void RemoveLayers();
  // Delete on cards: takes the picked dolls nothing uses out of the
  // project as one undo step.
  void TakeOut(const DollCards& cards);
  void Restack(int step);
  void PickLayer();
  void ToggleShown(QListWidgetItem* item);
  // Picked library dolls that are (or are not yet) in the project.
  QStringList PickedDolls(bool is_in_project) const;
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  // Library dolls re-exported since they were last taken.
  QSet<QString> stale_;
  // The pictures of library dolls, drawn once until the library or a
  // doll's art changes.
  QHash<QString, QPixmap> pictures_;
  QVBoxLayout layout_;
  // The Library section's title plate (it names unconverted dolls).
  QLabel* library_plate_ = nullptr;
  LibraryNavBar* nav_ = nullptr;
  DollCards library_;
  QHBoxLayout library_buttons_;
  QPushButton import_;
  QPushButton reload_;
  QPushButton place_;
  DollCards objects_;
  QHBoxLayout object_buttons_;
  QPushButton add_objects_;
  QPushButton background_;
  FoldSection* layers_fold_ = nullptr;
  QListWidget layers_;
  QHBoxLayout layer_buttons_;
  QToolButton add_;
  QMenu add_menu_;
  QPushButton copy_;
  QPushButton remove_;
  QPushButton up_;
  QPushButton down_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CAST_PANEL_H_
