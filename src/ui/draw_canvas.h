#ifndef SNAPPER_UI_DRAW_CANVAS_H_
#define SNAPPER_UI_DRAW_CANVAS_H_

#include <QHBoxLayout>
#include <QPointF>
#include <QTransform>
#include <QVBoxLayout>
#include <QWidget>

#include "render/image_cache.h"
#include "ui/draw_bar.h"
#include "ui/managers.h"

namespace snapper {

// Draw mode on the stage: while DrawManager draws, it covers the widget
// it was made over (the stage) with the piece at its rest place, the
// rest of the doll dimmed around it, a thin frame at the drawing's size
// (red while a stroke goes past it, where it is cut off) and pixel
// lines past 800 %. The wheel zooms toward the cursor, the middle
// button slides the view and Shift+1 fits the piece. It turns presses
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
  // Doll space to widget pixels.
  QTransform View() const;
  // The drawing's pixels to widget pixels.
  QTransform World() const;
  // The drawing pixel under point.
  QPoint PixelAt(QPointF point) const;
  void Fit();
  // The other pieces of the doll at rest, dimmed.
  void PaintDoll(QPainter* painter);
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
  ImageCache cache_;
  // Widget pixels per doll pixel, and the doll point at the middle.
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
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_CANVAS_H_
