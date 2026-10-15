#ifndef SNAPPER_UI_PALETTE_PANEL_H_
#define SNAPPER_UI_PALETTE_PANEL_H_

#include <QColor>
#include <QLabel>
#include <QPointer>
#include <QPoint>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#include "ui/color_wheel.h"
#include "ui/swatch_grid.h"

namespace snapper {

class FoldSection;
class MotionManager;
class PaletteManager;

// The floating palette: one small square-cornered panel over the
// window, moved by dragging it anywhere, resized by its bottom right
// corner, foldable to the current color, opened where and as big as it
// was last left. Under the title: a color wheel, the recent colors and
// the doll's colors (the picked piece's, the whole doll's and the
// hand-added ones), each a fold that stays as it was left. The doll's
// show only when they fit what the palette is open for
// (PaletteManager::ShowsDoll). Hovering a swatch makes the stage flash
// where the color is used. The Pick button is an eyedropper: the next
// click anywhere in the window picks the color under it (Escape or a
// right click cancels).
class PalettePanel final : public QWidget {
  Q_OBJECT

 public:
  // settings_path keeps where the panel was left and its folds.
  PalettePanel(PaletteManager* palette, MotionManager* motion,
               QString settings_path, QWidget* window);

  // Opens or closes it, where it was last left.
  void SetOpen(bool is_open);
  // Draw mode docks it: it lies exactly over area (the inspector, idle
  // while drawing) and follows it, with no drag or resize, so nothing
  // floats over the canvas. Undock lets it float where it was left.
  void DockOver(QWidget* area);
  void Undock();

 signals:
  void OpenChanged(bool is_open);

 protected:
  bool event(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  // Size, cursor and mouse tracking (ui/palette_panel_frame.cpp).
  void SetUpFrame();
  void BuildTitle();
  // The eyedropper (ui/palette_eyedropper.cpp).
  void StartEyedropper();
  void StopEyedropper();
  // Handles a mouse event while the eyedropper is on; false when off.
  bool Eyedrop(const QMouseEvent& event);
  // The window's color at global, as drawn; invalid off the window.
  QColor ColorAt(QPoint global) const;
  void ShowCurrent(const QColor& color);
  void BuildSections(MotionManager* motion);
  void Refresh();
  void ShowGrid(SwatchGrid* grid, QLabel* title, std::vector<QColor> colors,
                bool is_shown);
  void SetFolded(bool is_folded);
  void ShowMenu(const QColor& color, const QPoint& global, bool is_added);
  void AddColor();
  void KeepPlace();
  void RestorePlace();
  // The recent colors in the settings file, shared by every project.
  void KeepRecent() const;
  void LoadRecent();
  // The panel as tall as its colors need at its width, or as tall as
  // the user made it when that is more.
  void FitHeight();
  // Lies over the docking area, as big as it is
  // (ui/palette_panel_dock.cpp).
  void Follow();
  // Docked and the area shows: a hidden area (full view) leaves it
  // floating until the area comes back.
  bool is_docked() const {
    return !dock_area_.isNull() && dock_area_->isVisible();
  }
  // True when at (panel pixels) is on the resize corner.
  bool IsOnGrip(QPoint at) const;
  // The swatch under at (panel pixels) in any shown grid; invalid for
  // none.
  QColor SwatchAt(QPoint at) const;

  PaletteManager* palette_;
  QString settings_path_;
  // The area it is docked over while drawing; null while it floats.
  QPointer<QWidget> dock_area_;
  QVBoxLayout layout_;
  QWidget title_;
  QLabel title_text_;
  QToolButton pick_;
  QToolButton fold_;
  QToolButton close_;
  // The folds; the panel's children, kept to show and hide.
  FoldSection* wheel_fold_ = nullptr;
  FoldSection* recent_fold_ = nullptr;
  FoldSection* doll_fold_ = nullptr;
  ColorWheel* wheel_ = nullptr;
  SwatchGrid* recent_ = nullptr;
  QLabel* recent_empty_ = nullptr;
  QLabel* piece_title_ = nullptr;
  SwatchGrid* piece_ = nullptr;
  QLabel* doll_title_ = nullptr;
  SwatchGrid* doll_ = nullptr;
  QLabel* added_title_ = nullptr;
  SwatchGrid* added_ = nullptr;
  QWidget current_row_;
  QLabel current_swatch_;
  QLabel current_hex_;
  bool is_folded_ = false;
  bool is_eyedropping_ = false;
  // A left press on the panel (or a swatch): where it was grabbed, and
  // whether it has moved far enough to be a drag of the panel.
  bool is_pressed_ = false;
  bool is_moving_ = false;
  QPoint grab_;
  Qt::KeyboardModifiers press_keys_;
  // The corner is being dragged: the size and cursor it started from.
  bool is_resizing_ = false;
  QSize resize_from_;
  QPoint resize_at_;
  // The height the user asked for by resizing; 0 for as tall as needed.
  int asked_height_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_PALETTE_PANEL_H_
