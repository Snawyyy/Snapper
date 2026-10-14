#ifndef SNAPPER_UI_COMP_STAGE_H_
#define SNAPPER_UI_COMP_STAGE_H_

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <map>
#include <memory>
#include <optional>

#include "base/frame.h"
#include "edit/video_file_frames.h"
#include "edit/edit_scope.h"
#include "model/comp.h"
#include "render/frame_renderer.h"
#include "ui/managers.h"
#include "ui/view_camera.h"

class QPainter;

namespace snapper {

// The Comp tab's stage: the reel's picture at the reel playhead (the
// Video tab's video, with every shot's comp), fitted to the view, through
// the one renderer, so it matches the export. The effects it edits are
// those of the shot on top there. Frames are kept by reel frame until the project, V or K changes, so
// scrubbing back is instant. While an effect is picked its yellow
// handles show: points A and B, and an arrow for a light or a direction;
// dragging one is one undo step, Escape cancels it. A click elsewhere
// picks the layer (and piece) under it, outlined in red, and the effect
// list shows its effects; a click on nothing picks the screen.
class CompStage final : public QWidget {
  Q_OBJECT

 public:
  explicit CompStage(const Managers& managers);

  // Names a blend mode over the stage for a moment.
  void ShowChip(const QString& text);
  // The color of the frame without effects under the cursor; invalid
  // when the cursor is off the frame.
  QColor ColorUnderCursor();

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  enum class Handle { kNone, kA, kB, kArrow };

  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }

  // Where the frame sits in the widget and how big it is drawn.
  QRectF FrameRect() const;
  // The scale that fits the whole frame, the one drawn (fit and zoom),
  // and the one the picture is rendered at.
  double FitScale() const;
  double Scale() const;
  double RenderScale() const;
  // The picture for the playhead, from the kept frames or drawn now.
  QImage Picture();
  void Forget();
  // Handle positions in widget pixels; nothing when not shown.
  std::optional<QPointF> HandleAt(Handle handle);
  QPointF ArrowCenter() const;
  Handle HandleUnder(QPointF point);
  void DragTo(QPointF point, bool is_snapped);
  // A click picks the layer (and piece) under it, so a new effect goes
  // on it; a click on nothing picks the screen.
  void PickUnder(QPointF point, bool is_adding);
  // The Teto red outline of every pick (ui/comp_stage_picks.cpp).
  void PaintPicks(QPainter* painter);
  // The picked effect of the shot being worked in, or nullptr.
  const CompEffect* Picked() const;
  Frame Local() const;

  Managers managers_;
  FrameRenderer renderer_;
  VideoFileFrames videos_;
  ViewCamera camera_;
  std::map<int, QImage> kept_;
  double kept_scale_ = 0.0;
  Handle dragging_ = Handle::kNone;
  std::unique_ptr<EditScope> drag_;
  QString chip_;
  QTimer chip_timer_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_STAGE_H_
