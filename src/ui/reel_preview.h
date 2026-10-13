#ifndef SNAPPER_UI_REEL_PREVIEW_H_
#define SNAPPER_UI_REEL_PREVIEW_H_

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <memory>

#include "edit/edit_scope.h"
#include "edit/video_file_frames.h"
#include "model/reel.h"
#include "render/frame_renderer.h"
#include "ui/inline_text_edit.h"
#include "ui/managers.h"
#include "ui/view_camera.h"

namespace snapper {

// The final video at the reel's playhead, as big as fits. Shots are
// posed on the Pose tab and clips cut on the timeline below; only text
// clips are handled here: click one showing to pick it, drag it to move
// it (it catches on the frame's middle and the title-safe margin; Ctrl
// turns that off), drag a yellow corner to scale it, double click to
// type in it. Each drag is one step; Escape cancels it.
class ReelPreview final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelPreview(const Managers& managers);

  // Opens the typing box over clip's words.
  void EditText(ClipId clip);

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { kNone, kMove, kScale };

  // Output pixels per canvas pixel that fit the whole frame, as drawn
  // (fit and zoom), the frame's drawn size, and where its corner is.
  double FitScale() const;
  double Scale() const;
  QSizeF FrameSize() const;
  QPointF Corner() const;
  // A text clip's lettering box on screen; empty when it is not showing.
  QRectF ScreenRect(ClipId clip) const;
  // The topmost text clip showing under at, or an invalid id.
  ClipId TextAt(QPointF at) const;
  // The corner handle of a picked text clip under at: its clip, or an
  // invalid id.
  ClipId HandleAt(QPointF at) const;
  void PaintPicked(QPainter* painter) const;
  void PaintGuides(QPainter* painter) const;
  void Follow(QPointF at, Qt::KeyboardModifiers modifiers);
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  ViewCamera camera_;
  FrameRenderer renderer_;
  VideoFileFrames videos_;
  InlineTextEdit typing_;
  Drag drag_ = Drag::kNone;
  ClipId dragged_;
  QPointF press_;
  bool has_moved_ = false;
  // The dragged text's place and size as the drag began.
  QPointF start_place_;
  double start_size_ = 0.0;
  // The guides the drag caught on, shown while it lasts.
  bool is_snapped_x_ = false;
  bool is_snapped_y_ = false;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_PREVIEW_H_
