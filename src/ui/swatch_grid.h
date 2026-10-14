#ifndef SNAPPER_UI_SWATCH_GRID_H_
#define SNAPPER_UI_SWATCH_GRID_H_

#include <QColor>
#include <QPoint>
#include <QSize>
#include <QWidget>

#include <vector>

namespace snapper {

// A grid of color swatches. Reports the swatch under the cursor and a
// right click; decides nothing. A left press goes on to the panel, which
// picks the swatch on release unless the press became a drag of the
// panel.
class SwatchGrid final : public QWidget {
  Q_OBJECT

 public:
  explicit SwatchGrid(QWidget* parent);

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

}  // namespace snapper

#endif  // SNAPPER_UI_SWATCH_GRID_H_
