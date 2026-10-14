#ifndef SNAPPER_EDIT_PALETTE_MANAGER_H_
#define SNAPPER_EDIT_PALETTE_MANAGER_H_

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <map>
#include <vector>

#include "base/error.h"
#include "model/project.h"
#include "render/drawing_colors.h"

namespace snapper {

class HistoryManager;
class SelectionManager;

// What the palette is open for, which decides the sections it shows:
// the color wheel and Recent always, the doll's colors only for doll
// work (drawing on a piece, joint circles). kAuto shows the doll's
// while a doll is picked.
enum class PaletteUse { kAuto, kFree, kDoll };

// The floating palette's colors, shared by every feature that asks for
// a color: the picked piece's colors, the whole doll's, the recent
// ones and the doll's hand-added ones. It reads colors and never
// changes a drawing. Users of a color store it as a value; picking one
// is part of their own edit.
// Also holds the palette's current and hovered color, so the panel and
// the stage (flash, pick with I) agree.
class PaletteManager final : public QObject {
  Q_OBJECT

 public:
  PaletteManager(HistoryManager* history, SelectionManager* selection);

  // The doll the palette is about: the focused layer's, or empty.
  QString DollName() const;
  // Every solid color of the picked piece's drawings, most used first;
  // empty without a picked piece.
  std::vector<QColor> PieceColors() const;
  // Every solid color of the doll's drawings, most used first.
  std::vector<QColor> DollColors() const;
  // The colors last chosen, newest first: the current color each time
  // the palette closed after a pick. The panel keeps them in the
  // settings file between runs.
  const std::vector<QColor>& RecentColors() const { return recent_; }
  void SetRecentColors(std::vector<QColor> colors);
  // The doll's hand-added colors.
  std::vector<QColor> AddedColors() const;

  // Why AddColor can't act, for a greyed-out menu entry; empty when it
  // can.
  QString WhyNoAdd() const;
  // Adds color to the doll's palette; one undo step.
  Result<void> AddColor(const QColor& color);
  // Takes a hand-added color off the doll; one undo step.
  Result<void> RemoveColor(const QColor& color);

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
  // True when the doll's colors are worth showing now.
  bool ShowsDoll() const;

 signals:
  // The colors to show may have changed (pick, project, art).
  void Changed();
  void Picked(const QColor& color, Qt::KeyboardModifiers modifiers);
  void HoverChanged();
  void ShownChanged(bool is_shown);

 private:
  const Doll* PickedDoll() const;
  // The first picked piece of the focused layer; empty for none.
  QString PickedPiece() const;
  // A drawing's colors, read once and kept until the file changes.
  const std::vector<ColorCount>& ColorsOf(const QString& path) const;
  Result<void> WriteAdded(const QString& label, std::vector<QColor> colors);

  HistoryManager* history_;
  SelectionManager* selection_;
  QColor current_ = Qt::black;
  QColor hovered_;
  bool is_shown_ = false;
  // A color was picked since the palette opened.
  bool is_chosen_ = false;
  std::vector<QColor> recent_;
  PaletteUse use_ = PaletteUse::kAuto;
  struct Cached final {
    QDateTime stamp;
    std::vector<ColorCount> colors;
  };
  mutable std::map<QString, Cached> cache_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_PALETTE_MANAGER_H_
