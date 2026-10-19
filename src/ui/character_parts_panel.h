#ifndef SNAPPER_UI_CHARACTER_PARTS_PANEL_H_
#define SNAPPER_UI_CHARACTER_PARTS_PANEL_H_

#include <QComboBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

#include "base/error.h"
#include "model/character.h"
#include "ui/character_session.h"

namespace snapper {

class CharacterManager;
class HistoryManager;

// The tree of a character's parts, front first as in Moho's layers
// panel: pick, rename in place, show or hide, drag to reorder or move
// into a group, and the buttons that add parts.
class PartTree final : public QTreeWidget {
  Q_OBJECT

 public:
  explicit PartTree(QWidget* parent = nullptr);

 signals:
  // item was dropped under parent (none: the top) at row rows from the
  // front.
  void Dropped(PartId part, PartId parent, int row);

 protected:
  void dropEvent(QDropEvent* event) override;
};

// The Character tab's left panel: which character, and its parts.
class CharacterPartsPanel final : public QWidget {
  Q_OBJECT

 public:
  CharacterPartsPanel(HistoryManager* history, CharacterManager* characters,
                      CharacterSession* session, QWidget* parent = nullptr);

 signals:
  void Problem(const QString& message);

 private:
  void Refresh();
  void RefreshCharacters();
  void FillTree(const Character& character, PartId parent,
                QTreeWidgetItem* under);
  void AddPart(PartKind kind);
  void AddPicture();
  void Report(const Result<void>& done);

  HistoryManager* history_;
  CharacterManager* characters_;
  CharacterSession* session_;
  QComboBox picker_;
  QPushButton new_button_;
  QPushButton save_button_;
  PartTree tree_;
  QPushButton add_drawing_;
  QPushButton add_bones_;
  QPushButton add_group_;
  QPushButton add_switch_;
  QPushButton add_picture_;
  QPushButton duplicate_;
  QPushButton remove_;
  bool is_filling_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_PARTS_PANEL_H_
