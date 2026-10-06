#ifndef SNAPPER_UI_STAGE_VIEW_H_
#define SNAPPER_UI_STAGE_VIEW_H_

#include <QImage>
#include <QString>
#include <QWidget>

#include <optional>

#include "render/frame_renderer.h"
#include "render/stage_geometry.h"
#include "ui/managers.h"
#include "ui/pose_tool.h"

namespace snapper {

// The stage: the frame under the playhead, fitted to the widget, with
// the pose tool on top. Picks are outlined in Teto red; joints and IK
// tips are yellow handles, and a doll picked whole has yellow lean and
// swivel handles on its right side and below it. A picked piece with a
// warp grid shows it as faint lines with yellow dots to drag; clicking
// a dot picks it, and the wheel then sets how far it pulls its
// neighbours.
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
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void PaintHandles(const StageFrame& frame, QPainter* painter);
  // A picked piece's warp grid: faint lines and yellow dots.
  void PaintWarp(const StageFrame& frame, const Pick& pick,
                 QPainter* painter);
  // The picked warp dot: an orange ring, its rubber reach and a label.
  void PaintReach(const WarpOnScreen& warp, int point, QPainter* painter);
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
