#ifndef SNAPPER_UI_CHARACTER_PAGE_H_
#define SNAPPER_UI_CHARACTER_PAGE_H_

#include <QButtonGroup>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QWidget>

#include <array>

#include "ui/character_canvas.h"
#include "ui/character_inspector.h"
#include "ui/character_parts_panel.h"
#include "ui/character_session.h"
#include "ui/grip_splitter.h"

namespace snapper {

class CharacterManager;
class HistoryManager;

// The Character tab, laid out as Moho is: the tools along the top, the
// parts on the left, the canvas in the middle and the pick's settings on
// the right. Tool keys work while the canvas has the focus.
class CharacterPage final : public QWidget {
  Q_OBJECT

 public:
  CharacterPage(HistoryManager* history, CharacterManager* characters,
                const QString& settings_path, QWidget* parent = nullptr);

  CharacterSession* session() { return &session_; }
  CharacterCanvas* canvas() { return &canvas_; }

 signals:
  void Problem(const QString& message);

 protected:
  void keyPressEvent(QKeyEvent* event) override;

 private:
  QWidget* BuildTools();
  // Greys out the tools the picked part cannot use, saying why.
  void RefreshTools();

  HistoryManager* history_;
  CharacterSession session_;
  // Holders first: what they hold is destroyed before them and leaves.
  GripSplitter split_;
  QScrollArea inspector_scroll_;
  CharacterCanvas canvas_;
  CharacterPartsPanel parts_;
  CharacterInspector inspector_;
  QButtonGroup tool_group_;
  std::array<QAbstractButton*, kCharacterToolCount> tool_buttons_{};
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_PAGE_H_
