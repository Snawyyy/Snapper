#ifndef SNAPPER_UI_CAST_PANEL_H_
#define SNAPPER_UI_CAST_PANEL_H_

#include <QHBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/character_cards.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// Who is in the picked shot, in two tabs stacked in one area; the open
// tab is remembered. Characters: the character library as cards: make a
// new one, open one on the Character tab (double click), or put it on
// the stage (drag, or Put on stage). Layers: the shot's layers, top
// first: pick, tick to show or hide, add pictures, text, storyboards and
// effects, copy, delete and restack. Every list picks by the one pick
// rule (PickList); Delete on layers deletes them.
class CastPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit CastPanel(const Managers& managers);

 signals:
  void Problem(const QString& why);
  // Open that character on the Character tab (it is in the project).
  void CharacterOpenAsked(const QString& character);

 private:
  void BuildLayout();
  QWidget* MakeCharactersPage();
  QWidget* MakeLayersPage();
  // Keeps the open tab for the next start.
  void RememberTab(int tab) const;
  void Wire();
  void RefreshCharacters();
  // The card picture of a character: drawn fresh for project ones, read
  // from the library otherwise.
  QPixmap PictureOf(const QString& name) const;
  void RefreshLayers();
  void RefreshButtons();
  QStringList PickedCharacters() const;
  void NewCharacter();
  // Takes the character into the project when it is not in, then asks
  // for it to open.
  void OpenCharacter(const QString& name);
  void Place();
  void AddPicture();
  void AddBackground();
  void AddText();
  void RemoveLayers();
  void Restack(int step);
  void PickLayer();
  void ToggleShown(QListWidgetItem* item);
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  QVBoxLayout layout_;
  QTabWidget* tabs_ = nullptr;
  CharacterCards characters_;
  QHBoxLayout character_buttons_;
  QPushButton new_;
  QPushButton edit_;
  QPushButton place_;
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
