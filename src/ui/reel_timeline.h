#ifndef SNAPPER_UI_REEL_TIMELINE_H_
#define SNAPPER_UI_REEL_TIMELINE_H_

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <memory>
#include <set>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "model/reel.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// The reel, the final video, as an editor's timeline: time runs across,
// one row per track with the top track drawn on top, and a ruler in
// seconds with the playhead.
//
// Drop a shot from the shot list or a video file onto a track to place
// it there. Click a clip to pick it (Shift adds, Ctrl flips); drag it
// to move it, along or to another track; drag its ends to trim it.
// Drags snap to other clips' cuts and the playhead. Click or drag the
// ruler to move the playhead. S splits the picked clip at the playhead,
// Delete removes the picked clips; right-click for the same and for
// tracks. The wheel scrolls, Ctrl zooms. Shots here are finished
// videos: what is inside them is edited on the Pose tab.
class ReelTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelTimeline(const Managers& managers);

  // Where clip is drawn; empty when it is gone.
  QRectF ClipRect(ClipId clip) const;
  // The track drawn at y, or -1 off the tracks.
  int TrackAt(double y) const;
  // The reel frame at x, never before 0.
  Frame FrameAt(double x) const;
  double XOf(Frame frame) const;
  double TrackTop(int track) const;
  // Pixels per frame.
  double zoom() const { return zoom_; }
  void SetZoom(double pixels_per_frame);

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  enum class Drag { kNone, kSeek, kMove, kTrimStart, kTrimEnd };

  static constexpr double kHeaderWidth = 48.0;
  static constexpr double kRulerHeight = 24.0;
  static constexpr double kTrackHeight = 46.0;

  void PaintRuler(QPainter* painter) const;
  void PaintTracks(QPainter* painter) const;
  void PaintClip(QPainter* painter, const Clip& clip) const;
  void PaintDrop(QPainter* painter) const;
  void PaintPlayhead(QPainter* painter) const;
  // Seconds between ruler numbers at the current zoom.
  int LabelStep() const;

  // The clip under at, or an invalid id.
  ClipId ClipAt(QPointF at) const;
  // Which drag pressing at on clip starts: an end or the body.
  Drag DragFor(ClipId clip, QPointF at) const;
  void StartDrag(ClipId clip, Drag drag, QPointF at);
  void Follow(QPointF at);
  void Pick(ClipId clip, Qt::KeyboardModifiers modifiers);
  std::vector<ClipId> Picked() const;
  void SplitPicked();
  void ClipMenu(QPoint where);
  void TrackMenu(int track, QPoint where);
  void SeekTo(double x);
  // How many frames of snapping reach the zoom gives.
  int SnapReach() const;
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  double zoom_;
  // Frames scrolled off the left.
  double scroll_ = 0.0;
  Drag drag_ = Drag::kNone;
  ClipId dragged_;
  QPointF press_;
  // A plain click on a clip already in a bigger pick picks just it, once
  // it is clear the click was not a drag.
  bool is_plain_press_ = false;
  bool has_moved_ = false;
  std::unique_ptr<EditScope> scope_;
  // Where a dragged shot or file would land, while one hovers.
  int drop_track_ = -1;
  Frame drop_frame_;
  Frame drop_length_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_TIMELINE_H_
