#ifndef SNAPPER_UI_STAGE_VIEW_H_
#define SNAPPER_UI_STAGE_VIEW_H_

#include <QImage>
#include <QString>
#include <QMenu>
#include <QWidget>

#include <optional>

#include "render/frame_renderer.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/drag_feedback.h"
#include "ui/handle_hover.h"
#include "ui/managers.h"
#include "ui/pose_tool.h"

namespace snapper {

// The stage: the frame under the playhead, fitted to the widget, with
// the pose tool on top. Picks are outlined in Teto red; joints and IK
// tips are yellow handles, and a doll picked whole has yellow lean and
// swivel handles on its right side and below it. A picked piece with a
// warp grid shows it as faint lines with yellow dots to drag; clicking
// a dot picks it, and the wheel then sets how far it pulls its
// neighbours. Right-clicking something offers to link the picked
// things' movement to it, or to unlink them. Before any press, a thin
// dashed outline shows the thing a click would pick, the handle under
// the cursor grows and the cursor says what dragging it does. While a
// drag runs, a chip beside the cursor says its value, a faint ghost
// marks where it began, a snap flashes, and letting go pops.
class StageView final : public QWidget {
  Q_OBJECT

 public:
  explicit StageView(const Managers& managers);

  // Where the frame under the playhead is drawn; nothing when the
  // playhead is past every shot.
  std::optional<StageFrame> CurrentFrame() const;
  // The chip, ghost and pop of the drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }

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
  void leaveEvent(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  void PaintHandles(const StageFrame& frame, QPainter* painter);
  // The thin outline of the thing under the cursor.
  void PaintHover(const StageFrame& frame, QPainter* painter);
  // A doll's lean and swivel bars.
  void PaintTurn(const TurnHandles& turn, LayerId layer, QPainter* painter);
  // Follows the cursor to the handle or thing under it.
  void UpdateHover(QPointF point);
  void ClearHover();
  // A picked piece's warp grid: faint lines and yellow dots.
  void PaintWarp(const StageFrame& frame, const Pick& pick,
                 QPainter* painter);
  // The picked warp dot: an orange ring, its rubber reach and a label.
  void PaintReach(const WarpOnScreen& warp, int point, QPainter* painter);
  void ReportTool();
  // As a drag is grabbed: leaves a faint copy of the picked pieces or
  // layers, drawn by the frame renderer, where they sit now.
  void GrabGhost(const StageFrame& frame);
  // Shows what the drag in progress says: chip, ghost and snap flash.
  void ShowDrag();
  // Shows the angle the wheel has turned the picked things by since it
  // last rested.
  void ShowWheel(QPointF cursor, int notches, bool is_fine);
  // Ctrl+A: every piece of the picked doll, or every layer.
  void PickAll();
  // The right-click menu's link actions, for the thing at point; empty
  // when there is nothing to offer.
  void AddLinkActions(QMenu* menu, QPointF point, const StageFrame& frame);

  Managers managers_;
  FrameRenderer renderer_;
  PoseTool tool_;
  HandleHover hover_;
  DragFeedback feedback_;
  std::optional<Pick> hover_pick_;
  // Wheel movement not yet a whole notch.
  int wheel_left_ = 0;
  // Degrees turned by the wheel since the chip last faded.
  double wheel_degrees_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STAGE_VIEW_H_
