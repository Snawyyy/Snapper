#ifndef SNAPPER_UI_STAGE_VIEW_H_
#define SNAPPER_UI_STAGE_VIEW_H_

#include <QImage>
#include <QString>
#include <QWidget>

#include <optional>

#include "render/frame_renderer.h"
#include "ui/managers.h"
#include "ui/pose_tool.h"

namespace snapper {

// The stage: the frame under the playhead, fitted to the widget, with
// the pose tool on top. Picks are outlined in Teto red; joints and IK
// tips are yellow handles.
class StageView final : public QWidget {
  Q_OBJECT

 public:
  explicit StageView(const Managers& managers);

  // Where the frame under the playhead is drawn; nothing when the
  // playhead is past every shot.
  std::optional<StageFrame> CurrentFrame() const;

 signals:
  // Something the user tried didn't work; empty clears it.
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void PaintHandles(const StageFrame& frame, QPainter* painter);
  void ReportTool();
  // Ctrl+A: every piece of the picked doll, or every layer.
  void PickAll();

  Managers managers_;
  FrameRenderer renderer_;
  PoseTool tool_;
  // Wheel movement not yet a whole notch.
  int wheel_left_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STAGE_VIEW_H_
