#ifndef SNAPPER_UI_REEL_TIMELINE_H_
#define SNAPPER_UI_REEL_TIMELINE_H_

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>

class QKeyEvent;
class QMenu;

#include <memory>
#include <set>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "model/reel.h"
#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// The reel, the final video, as an editor's timeline: time runs across,
// one row per track with the top track drawn on top, and a ruler in
// seconds with the playhead.
//
// Drop a shot from the shot list or a video file onto a track to place
// it there. Click a clip to pick it (Shift adds, Ctrl flips), drag a
// box over empty space to pick what it touches, Ctrl+A picks all and
// Escape or a click on empty space clears. Drag a clip to move it,
// along or to another track; drag its ends to trim it. Drags snap to
// other clips' cuts, cut markers and the playhead; Escape cancels one.
// Click or drag the ruler to move the playhead. Cut markers (Mark cut
// on the Video tab) show as yellow notches and dashed lines;
// double-click a track between two of them to pick which part of a
// video plays in that gap (SlotPicker). Double-click the cut between
// two touching clips to pick its transition (TransitionPicker).
// Keys: S splits at the playhead (the picked clips, or with none
// picked what the playhead is in), Delete removes, Shift+Delete
// removes and closes the gap, Ctrl+C, Ctrl+X and Ctrl+V copy, cut and
// paste at the playhead, Ctrl+D duplicates; right-click for the same
// and for tracks. The wheel scrolls, Ctrl zooms. Shots here are
// finished videos: what is inside them is edited on the Pose tab.
class ReelTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelTimeline(const Managers& managers);

  // Where clip is drawn; empty when it is gone.
  QRectF ClipRect(ClipId clip) const;
  // The clip whose end at is on, where the next clip on its track
  // starts: the cut a transition lives on. Invalid elsewhere.
  ClipId JointAt(QPointF at) const;
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
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  enum class Drag { kNone, kSeek, kMove, kTrimStart, kTrimEnd, kBox };
  // What a key or a menu entry asks of the picked clips.
  enum class Command {
    kNone,
    kSplit,
    kRemove,
    kRipple,
    kCopy,
    kCut,
    kPaste,
    kDuplicate,
  };

  static constexpr double kHeaderWidth = 48.0;
  static constexpr double kRulerHeight = 24.0;
  static constexpr double kTrackHeight = 46.0;

  void PaintRuler(QPainter* painter) const;
  void PaintTracks(QPainter* painter) const;
  void PaintClip(QPainter* painter, const Clip& clip) const;
  void PaintDrop(QPainter* painter) const;
  void PaintMarkers(QPainter* painter) const;
  void PaintPlayhead(QPainter* painter) const;
  void PaintBox(QPainter* painter) const;
  // Each cut between touching clips: a mark to double-click, and the
  // span its transition mixes over.
  void PaintJoints(QPainter* painter) const;
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
  // Picks the clips the box touches, or clears on a plain click.
  void FinishBox();
  // Escape: drops the drag in progress, or else the pick.
  void CancelOrClear();
  static Command CommandFor(const QKeyEvent& event);
  // Why command can't act now; empty when it can.
  QString WhyNot(Command command) const;
  void Run(Command command);
  void AddCommands(QMenu* menu);
  // Asks which part of a video fills the gap between the cut markers
  // around at on track, then fills it.
  void FillGap(int track, Frame at);
  // Asks how clip hands over to the next one, then sets it.
  void EditTransition(ClipId clip);
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
  // The rubber band while picking by box, and how it combines.
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  std::unique_ptr<EditScope> scope_;
  // Where a dragged shot or file would land, while one hovers.
  int drop_track_ = -1;
  Frame drop_frame_;
  Frame drop_length_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_TIMELINE_H_
