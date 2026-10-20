#ifndef SNAPPER_UI_CHARACTER_STYLES_PANEL_H_
#define SNAPPER_UI_CHARACTER_STYLES_PANEL_H_

#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include "model/character.h"
#include "ui/character_session.h"

namespace snapper {

class CharacterManager;
class HistoryManager;

// The Character tab's Styles section: the character's named styles,
// each with a swatch of its fill and line. New makes one from the look
// shown (and gives it to the picked shape); Use gives the picked style
// to the picked shape, or to new shapes when none is picked; Own look
// lets go of it; Delete removes it, its shapes keeping their look. A
// double click renames. Every change goes through CharacterManager.
class CharacterStylesPanel final : public QWidget {
  Q_OBJECT

 public:
  CharacterStylesPanel(HistoryManager* history, CharacterManager* characters,
                       CharacterSession* session, QWidget* parent = nullptr);

 signals:
  void Problem(const QString& message);

 private:
  void Refresh();
  void New();
  void Use();
  void OwnLook();
  void Delete();
  void Rename(QListWidgetItem* item);
  // The style picked in the list, or none.
  StyleId Picked() const;
  // The picked shape's index, when it is on the picked part.
  bool HasShape() const;
  const Character* Shown() const;
  void Report(const QString& problem) { emit Problem(problem); }

  HistoryManager* history_;
  CharacterManager* characters_;
  CharacterSession* session_;
  bool is_filling_ = false;
  QListWidget list_;
  QPushButton new_;
  QPushButton use_;
  QPushButton own_;
  QPushButton delete_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_STYLES_PANEL_H_
