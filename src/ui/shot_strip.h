#ifndef SNAPPER_UI_SHOT_STRIP_H_
#define SNAPPER_UI_SHOT_STRIP_H_

#include <QRectF>
#include <QString>
#include <QWidget>

#include <memory>
#include <vector>

#include "base/error.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "model/shot.h"
#include "ui/drag_feedback.h"
#include "ui/handle_hover.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// The master track as a strip of shots, each as wide as it is long.
// Click picks a shot and jumps the playhead to it; drag a shot to
// reorder it, drag its right edge to change its length; the + block
// adds a shot after the picked one; right-click for rename, copy,
// colour and delete. A drag shows a chip (frames added, or the place a
// shot would land), a faint ghost where it began, a flash per frame
// step and a pop on letting go; Escape drops it. Transitions live on
// the reel (Video tab).
class ShotStrip final : public QWidget {
  Q_OBJECT

 public:
  explicit ShotStrip(const Managers& managers);

  struct Block final {
    ShotId shot;
    QRectF rect;
  };
  // Where each shot is drawn, left to right, and the + block.
  std::vector<Block> Blocks() const;
  QRectF AddBlock() const;
  // The chip, ghost and flash of the drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { kNone, kMove, kLength };

  // One shot's block: colour, outline, edge handle, name and length.
  void PaintBlock(QPainter* painter, const Block& block) const;
  // As a shot or its edge is grabbed: leaves a faint copy of the shot
  // blocks that drag, drawn by PaintBlock, where they sit now.
  void GrabGhost();

  void Pick(ShotId shot);
  // Where a shot dropped with the mouse at x would land, from 0.
  int DropIndex(double x) const;
  // Escape: drops the drag in progress; false when there was none.
  bool CancelDrag();
  void Menu(ShotId shot, QPoint where);
  std::vector<ShotId> PickedShots() const;
  void Report(const QString& problem) { emit Problem(problem); }
  // Removes shots; the toast offers Undo for exactly that step.
  void DeletePicked(const std::vector<ShotId>& picked);

  Managers managers_;
  // The right edge under the cursor widens and brightens.
  HandleHover hover_;
  DragFeedback feedback_;
  Drag drag_ = Drag::kNone;
  ShotId dragged_;
  double drag_start_x_ = 0.0;
  // True once the mouse has left the dead zone of the press.
  bool is_started_ = false;
  // Frames already added by the edge drag in progress.
  int drag_frames_ = 0;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_STRIP_H_
