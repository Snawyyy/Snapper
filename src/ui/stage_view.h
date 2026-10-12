#ifndef SNAPPER_UI_STAGE_VIEW_H_
#define SNAPPER_UI_STAGE_VIEW_H_

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QImage>
#include <QString>
#include <QMenu>
#include <QWidget>

#include <optional>

#include "edit/change_watcher.h"
#include "render/frame_renderer.h"
#include "render/stage_camera.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/drag_feedback.h"
#include "ui/handle_hover.h"
#include "ui/managers.h"
#include "ui/pose_tool.h"
#include "ui/stage_flash.h"
#include "ui/stage_poke.h"

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
// marks where it began, a snap flashes, and letting go pops. Undo and
// redo flash the layers they changed.
class StageView final : public QWidget {
  Q_OBJECT

 public:
  explicit StageView(const Managers& managers);

  // Where the frame under the playhead is drawn; nothing when the
  // playhead is past every shot.
  std::optional<StageFrame> CurrentFrame() const;
  // The chip, ghost and pop of the drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }
  // How the view looks at the frame (zoom and pan).
  const StageCamera& camera() const { return camera_; }
  // Ctrl+wheel zooms toward the cursor; the middle button drags the view
  // along; Shift+1 fits the frame, Shift+2 frames the pick, Shift+0 is
  // actual size.
  void SetCamera(const StageCamera& camera);

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
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  // The arrow keys nudge the pick while the stage has focus, ahead of
  // the window's frame-step shortcuts; Tab steps the pick.
  bool event(QEvent* event) override;
  // A doll card dragged here is put on the stage; the frame glows while
  // it hovers.
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  bool focusNextPrevChild(bool next) override;

 private:
  void PaintHandles(const StageFrame& frame, QPainter* painter);
  // A red rope from each picked follower to what it follows, and back
  // from a picked leader to its followers.
  void PaintRopes(const StageFrame& frame, QPainter* painter);
  // The lines a move drag has caught on, across the whole view.
  void PaintGuides(QPainter* painter);
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
  // A ring round every picked dot of pick's piece.
  void PaintPickedDots(const StageFrame& frame, const Pick& pick,
                       const WarpOnScreen& warp, QPainter* painter);
  void ReportTool();
  // An arrow key: nudges the pick 1 pixel (10 with Shift); true when it
  // was one and something is picked.
  bool NudgeKey(QKeyEvent* event);
  // Escape: backs out of the drag, then the picked dot, then the pick.
  void BackOut();
  // Zooms by notches wheel notches toward cursor.
  void ZoomBy(double notches, QPointF cursor);
  // Shift+1, +2 or +0: fit, frame the pick, actual size. True when key
  // was one of them.
  bool CameraKey(QKeyEvent* event);
  // Says the zoom beside the cursor.
  void ShowZoom(QPointF cursor);
  // The box round the picked layers, in widget pixels; empty with none.
  QRectF PickBox(const StageFrame& frame);
  // As a drag is grabbed: leaves a faint copy of the picked pieces or
  // layers, drawn by the frame renderer, where they sit now.
  void GrabGhost(const StageFrame& frame);
  // After a step lands: undo or redo flashes the layers it changed.
  void ShowStep(const Project& before, bool is_travel);
  // Works out and starts the flash for the step from before.
  void BuildFlash(const Project& before);
  // Shows what the drag in progress says: chip, ghost and snap flash.
  void ShowDrag();
  // Shows the angle the wheel has turned the picked things by since it
  // last rested.
  void ShowWheel(QPointF cursor, int notches, bool is_fine);
  // P down or up: holding it arms poking (see StagePoke). Returns true
  // when the key was P and is dealt with.
  bool PokeKey(QKeyEvent* event, bool is_down);
  // The pixels of the palette's hovered color, lit up over image (drawn
  // at corner), so a swatch shows where its color is used.
  void PaintColorFlash(const QImage& image, QPointF corner,
                       QPainter* painter) const;
  // I with the palette open: picks the color under the cursor from the
  // frame last drawn. True when the key was dealt with.
  bool PaletteKey(QKeyEvent* event);
  // The layer a click at point would pick; invalid for none.
  LayerId LayerUnder(QPointF point, const StageFrame& frame) const;
  // Ctrl+A: every piece of the picked doll, or every layer.
  void PickAll();
  // The right-click menu's link actions, for the thing at point; empty
  // when there is nothing to offer.
  void AddLinkActions(QMenu* menu, QPointF point, const StageFrame& frame);

  Managers managers_;
  StageCamera camera_;
  // A doll card is over the stage.
  bool is_drop_target_ = false;
  // The middle button is dragging the view; where the mouse was.
  bool is_panning_ = false;
  QPointF pan_from_;
  FrameRenderer renderer_;
  // Draws the state before an undo or redo, for the flash.
  FrameRenderer was_renderer_;
  PoseTool tool_;
  StagePoke poke_;
  HandleHover hover_;
  DragFeedback feedback_;
  ChangeWatcher watcher_;
  StageFlash flash_;
  std::optional<Pick> hover_pick_;
  // The frame last drawn and where, for picking a color with I.
  QImage shown_;
  QPointF shown_corner_;
  // Wheel movement not yet a whole notch.
  int wheel_left_ = 0;
  // Degrees turned by the wheel since the chip last faded.
  double wheel_degrees_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STAGE_VIEW_H_
