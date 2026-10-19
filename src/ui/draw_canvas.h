#ifndef SNAPPER_UI_DRAW_CANVAS_H_
#define SNAPPER_UI_DRAW_CANVAS_H_

#include <QHBoxLayout>
#include <QPointF>
#include <QTransform>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/draw_bar.h"
#include "ui/managers.h"

namespace snapper {

// Draw mode on the stage: while DrawManager draws, it covers the widget
// it was made over (the stage) with the picture over the shot dimmed
// under it, a thin frame at the picture's size (red while a stroke goes
// past it, where it is cut off) and pixel lines past 800 %. The wheel
// zooms toward the cursor, the middle button slides the view and
// Shift+1 fits the picture. It turns presses
// and keys into DrawManager calls and decides nothing itself.
class DrawCanvas final : public QWidget {
  Q_OBJECT

 public:
  DrawCanvas(const Managers& managers, QWidget* over);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void leaveEvent(QEvent* event) override;
  // Takes draw mode's keys ahead of the window's shortcuts.
  bool event(QEvent* event) override;
  // Follows the size of the widget it covers.
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void Begin();
  void End();
  // The drawing's pixels to widget pixels.
  QTransform World() const;
  // The drawing pixel under point.
  QPoint PixelAt(QPointF point) const;
  void Fit();
  // The shot under the picture, dimmed.
  void PaintUnder(QPainter* painter);
  // The drawing, its onion or original, and its frame.
  void PaintDrawing(QPainter* painter);
  void PaintGrid(QPainter* painter);
  // The brush's pixels outlined under the cursor.
  void PaintCursor(QPainter* painter);
  // A key of draw mode; false when it is none.
  bool ToolKey(QKeyEvent* event);

  Managers managers_;
  QWidget* over_;
  QVBoxLayout layout_;
  DrawBar bar_;
  QHBoxLayout row_;
  DrawTools tools_;
  // Widget pixels per drawing pixel, and the drawing point at the
  // middle.
  double zoom_ = 1.0;
  QPointF center_;
  bool is_panning_ = false;
  QPointF pan_from_;
  bool is_stroking_ = false;
  // The stroke went past the drawing's edge.
  bool is_outside_ = false;
  bool has_cursor_ = false;
  QPointF cursor_;
  // Alt is held: any tool picks a color.
  bool is_alt_ = false;
  // Space is held: a left drag grabs the view instead of drawing.
  bool is_space_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_CANVAS_H_
