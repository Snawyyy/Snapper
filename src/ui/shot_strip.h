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
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// The master track as a strip of shots, each as wide as it is long.
// Click picks a shot and jumps the playhead to it; drag a shot to
// reorder it, drag its right edge to change its length; the + block
// adds a shot after the picked one; right-click for rename, copy,
// colour and delete. Transitions live on the reel (Video tab).
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

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { kNone, kMove, kLength };

  void Pick(ShotId shot);
  void Menu(ShotId shot, QPoint where);
  std::vector<ShotId> PickedShots() const;
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  Drag drag_ = Drag::kNone;
  ShotId dragged_;
  double drag_start_x_ = 0.0;
  // Frames already added by the edge drag in progress.
  int drag_frames_ = 0;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_STRIP_H_
