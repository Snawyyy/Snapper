#ifndef SNAPPER_UI_TIMELINE_VIEW_H_
#define SNAPPER_UI_TIMELINE_VIEW_H_

#include <QString>
#include <QToolButton>
#include <QWidget>

#include <memory>
#include <optional>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "edit/timeline_rows.h"
#include "ui/handle_hover.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// The picked shot's timeline, laid out as in traditional animation:
// frames run across, one row per doll (its whole pose), layer or the
// camera. A key is a diamond; a hold is a bar to the next key, an ease
// a thin line. The song's waveform and the playhead, tagged with its
// frame, run on top; the ruler numbers frames as closely as the zoom
// allows and faint lines mark each frame down the rows.
//
// Click a key to pick it (Shift adds), drag to slide; double-click
// empty space to key the pose as it is; right-click for ease, spacing
// picked keys evenly (on 2s, on 3s...), copy, paste and holds; Delete
// removes; the wheel scrolls, Ctrl zooms.
class TimelineView final : public QWidget {
  Q_OBJECT

 public:
  explicit TimelineView(const Managers& managers);

  // Shot frame under x, or nothing left of the frames.
  std::optional<Frame> FrameAt(double x) const;
  double XOf(Frame frame) const;

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  void PaintRuler(QPainter* painter) const;
  // Faint lines down the rows at each frame's start.
  void PaintGrid(QPainter* painter) const;
  // Frames between ruler numbers at the current zoom.
  int LabelStep() const;
  void PaintWave(QPainter* painter) const;
  void PaintRows(QPainter* painter, const std::vector<TimelineRow>& rows);
  void PaintPlayhead(QPainter* painter) const;
  // The row under y, or -1.
  int RowAt(double y, size_t count) const;
  // The key of row nearest to x within reach, or nothing.
  std::optional<Frame> KeyNear(const TimelineRow& row, double x) const;
  // Grows the key under the cursor and shows its cursor.
  void UpdateHover(QPointF point);
  void ClearHover();
  // The name the hover knows a key by.
  static QString KeyName(size_t row, Frame frame);
  // The playhead's frame in this shot, or nothing when it is elsewhere.
  std::optional<Frame> PlayheadHere() const;
  void Seek(Frame local);
  // Opens the one undo step of a key drag that starts at at.
  void StartKeyDrag(Frame key, QPointF at);
  void Report(const QString& problem) { emit Problem(problem); }
  // Removes the picked keys; the toast offers Undo for that step.
  void DeletePicked();
  void KeyMenu(const TimelineRow& row, Frame frame, QPoint where);
  void EmptyMenu(const TimelineRow& row, Frame frame, QPoint where);

  // Shows and flips the playback's step mode.
  void ShowStepMode();

  Managers managers_;
  // Scrub or animation stepping, in the ruler's corner.
  QToolButton step_mode_;
  double frame_width_;
  double scroll_ = 0.0;
  bool is_scrubbing_ = false;
  void FinishBox();

  // A box being drawn over keys, and how it combines with the pick.
  bool is_boxing_ = false;
  QPointF box_from_;
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  std::unique_ptr<EditScope> key_drag_;
  Frame drag_frame_;
  // Where a key was pressed; it slides only once the mouse has gone
  // kDragDeadZone, then jumps under the cursor.
  QPointF press_;
  bool is_started_ = false;
  HandleHover hover_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_VIEW_H_
