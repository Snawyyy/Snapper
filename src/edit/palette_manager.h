#ifndef SNAPPER_EDIT_PALETTE_MANAGER_H_
#define SNAPPER_EDIT_PALETTE_MANAGER_H_

#include <QColor>
#include <QObject>
#include <QString>

#include <vector>

#include "model/project.h"

namespace snapper {

class HistoryManager;
class SelectionManager;

// What the palette is open for, which decides the sections it shows:
// the color wheel and Recent always, the character's colors for
// character work. kAuto shows the character's while one is picked.
enum class PaletteUse { kAuto, kFree, kCharacter };

// The floating palette's colors, shared by every feature that asks for
// a color: the picked character's fill and line colors and the recent
// ones. It reads colors and never changes a drawing. Users of a color
// store it as a value; picking one is part of their own edit.
// Also holds the palette's current and hovered color, so the panel and
// the stage (flash, pick with I) agree.
class PaletteManager final : public QObject {
  Q_OBJECT

 public:
  PaletteManager(HistoryManager* history, SelectionManager* selection);

  // The character the palette is about: the focused layer's, or empty.
  QString CharacterName() const;
  // Every fill and line color of the character's shapes, most used
  // first.
  std::vector<QColor> CharacterColors() const;
  // The colors last chosen, newest first: the current color each time
  // the palette closed after a pick. The panel keeps them in the
  // settings file between runs.
  const std::vector<QColor>& RecentColors() const { return recent_; }
  void SetRecentColors(std::vector<QColor> colors);

  // The color last picked in the palette.
  QColor current() const { return current_; }
  // A swatch (or the stage, with I) was picked. Edits nothing: the
  // feature that asked for the color edits with it.
  void Pick(const QColor& color, Qt::KeyboardModifiers modifiers = {});
  // The swatch under the cursor, invalid for none; the stage flashes
  // where it is used.
  QColor hovered() const { return hovered_; }
  void Hover(const QColor& color);
  // Whether the panel is open; the stage takes I only while it is.
  bool is_shown() const { return is_shown_; }
  // Closing it puts its use back to kAuto and, after a pick, the
  // current color first in the recent ones.
  void SetShown(bool is_shown);
  // A feature about to ask for a color says what for.
  void SetUse(PaletteUse use);
  // True when the character's colors are worth showing now.
  bool ShowsCharacter() const;

 signals:
  // The colors to show may have changed (pick, project).
  void Changed();
  void Picked(const QColor& color, Qt::KeyboardModifiers modifiers);
  void HoverChanged();
  void ShownChanged(bool is_shown);

 private:
  const Character* PickedCharacter() const;

  HistoryManager* history_;
  SelectionManager* selection_;
  QColor current_ = Qt::black;
  QColor hovered_;
  bool is_shown_ = false;
  // A color was picked since the palette opened.
  bool is_chosen_ = false;
  std::vector<QColor> recent_;
  PaletteUse use_ = PaletteUse::kAuto;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_PALETTE_MANAGER_H_
