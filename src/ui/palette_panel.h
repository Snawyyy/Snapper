#ifndef SNAPPER_UI_PALETTE_PANEL_H_
#define SNAPPER_UI_PALETTE_PANEL_H_

#include <QColor>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace snapper {

class PaletteManager;

// A grid of color swatches. Reports the swatch under the cursor and a
// right click; decides nothing. A left press goes on to the panel, which
// picks the swatch on release unless the press became a drag of the
// panel.
class SwatchGrid final : public QWidget {
  Q_OBJECT

 public:
  SwatchGrid();

  void SetColors(std::vector<QColor> colors, const QColor& current);
  QSize sizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;
  // The color at point; invalid when no swatch is there.
  QColor ColorAt(QPoint point) const;

 signals:
  void Hovered(const QColor& color);
  void MenuAsked(const QColor& color, const QPoint& global);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  int Columns() const;

  std::vector<QColor> colors_;
  QColor current_;
  QColor hovered_;
};

// The floating palette: one small square-cornered panel over the
// window, moved by dragging it anywhere, resized by its bottom right
// corner, foldable to the current color, opened where and as big as it
// was last left.
// Tabs show the picked piece's colors, the doll's, the recent and the
// hand-added ones (see PaletteManager). Hovering a swatch makes the
// stage flash where the color is used.
class PalettePanel final : public QWidget {
  Q_OBJECT

 public:
  // settings_path keeps where the panel was left.
  PalettePanel(PaletteManager* palette, QString settings_path,
               QWidget* window);

  // Opens or closes it, where it was last left.
  void SetOpen(bool is_open);

 signals:
  void OpenChanged(bool is_open);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  enum class Tab { kPiece = 0, kDoll = 1, kRecent = 2, kAdded = 3 };

  void Refresh();
  void SetFolded(bool is_folded);
  void ShowMenu(const QColor& color, const QPoint& global);
  void AddColor();
  void KeepPlace();
  void RestorePlace();
  // The panel as tall as its colors need at its width, or as tall as
  // the user made it when that is more.
  void FitHeight();
  // True when at (panel pixels) is on the resize corner.
  bool IsOnGrip(QPoint at) const;
  Tab tab() const { return static_cast<Tab>(tabs_.currentIndex()); }

  PaletteManager* palette_;
  QString settings_path_;
  QVBoxLayout layout_;
  QWidget title_;
  QLabel title_text_;
  QToolButton fold_;
  QToolButton close_;
  QTabBar tabs_;
  QLabel empty_;
  SwatchGrid grid_;
  QWidget current_row_;
  QLabel current_swatch_;
  QLabel current_hex_;
  bool is_folded_ = false;
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
