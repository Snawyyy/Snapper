#ifndef SNAPPER_UI_COMP_TIMELINE_H_
#define SNAPPER_UI_COMP_TIMELINE_H_

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/frame.h"
#include "edit/comp_manager.h"
#include "edit/edit_scope.h"
#include "model/project.h"
#include "ui/managers.h"
#include "ui/playhead_settle.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_scale.h"

class QMenu;
class QPainter;

namespace snapper {

// The Comp tab's timeline: the Video tab's reel with its playhead, a
// row of the shot clips (the one under the playhead is the picked
// shot), then a row per effect of every shot on the reel, shot by shot
// and in the order they run, each a clip over the reel showing when it
// runs and the frames its knobs are keyed on, named with what it is on
// ("(arm_l) Rim light"), and one row for the picked shot's Light. The
// rows stay put as the playhead moves.
//
// Clips pick by the shared pick rule (Shift adds, Ctrl flips), a box
// dragged from empty space picks what it touches, Ctrl+A picks all,
// Escape drops the pick. Dragging a clip moves every picked clip with
// their keys, dragging an end trims it, both snap to shot cuts, other
// effect clips, reel markers and the playhead (Ctrl drags free); a chip
// beside the cursor says how many frames the drag went, a faint ghost
// marks where it began, a flash shows a snap and letting go pops
// (DragFeedback, as on the other tabs); Escape cancels and each drag is
// one undo step. Alt+Left and Alt+Right nudge the picked clips a frame
// (Shift: ten). A double click fits a clip to its shot again; right
// click offers the same with the Video tab's clip edits: S splits the
// picked clips at the playhead, Ctrl+X cuts, Ctrl+C copies, Ctrl+V
// pastes at the playhead, Ctrl+D duplicates, Delete removes. View,
// ruler, playhead and following are the Pose and Video tabs' too
// (TimelineScale, timeline_ruler.h): the wheel scrolls time,
// Ctrl+wheel zooms round the cursor, Ctrl+= and Ctrl+- zoom round the
// playhead, Ctrl+0 (or Show all in the menu) shows the whole reel,
// which is also how it starts. A
// clip may run past its shot's cut: an effect on the screen then goes
// on over whatever the reel shows; one on a doll stops at the cut (that
// part is hatched). A click on the
// ruler or the shot row moves the playhead, which picks the shot there.
class CompTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit CompTimeline(const Managers& managers);

  QSize sizeHint() const override { return QSize(600, 120); }
  // Delete: removes every picked clip, as one undo step.
  void RemovePicked();
  // The Video tab's clip edits on the picked clips, at the reel
  // playhead (ui/comp_timeline_keys.cpp): S, Ctrl+C, Ctrl+X, Ctrl+V and
  // Ctrl+D.
  void SplitPicked();
  // True when something was copied.
  bool CopyPicked();
  void CutPicked();
  void PasteAtPlayhead();
  void DuplicatePicked();

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  // One effect's clip as drawn: its box, and its frames of the shot.
  struct Bar final {
    CompClipRef ref{};
    // Shot frame f shows at reel frame f + offset.
    int offset = 0;
    const Clip* clip = nullptr;
    QRectF box;
    int first = 0;
    int end = 0;
    bool is_on_doll = false;
  };
  // A picked clip's frames when its drag began.
  struct Span final {
    int first = 0;
    int end = 0;
    int offset = 0;
  };
  enum class Grab { kNone, kMove, kStart, kEnd, kBox, kScrub };
  // The right click menu's entries; kNone is a separator.
  enum class MenuCommand {
    kNone,
    kSplit,
    kCopy,
    kCut,
    kPaste,
    kDuplicate,
    kFit,
    kNudgeBack,
    kNudgeOn,
    kRemove,
    kAll,
    kWholeReel,
  };

  // The names column (wide enough for "(arm_l) Rim light"), the ruler
  // and one row.
  static constexpr double kNameWidth = 180.0;
  static constexpr double kRuler = kRulerHeight;
  static constexpr double kRow = 18.0;

  // x of a reel frame, and the reel frame at x (unclamped when free),
  // through the zoom and scroll.
  double XOf(int frame) const;
  int FrameAt(double x) const;
  int FreeFrameAt(double x) const;
  double PixelsPerFrame() const { return scale_.pixels_per_frame(); }
  int Length() const;
  void SeekTo(double x);
  // Painting (ui/comp_timeline.cpp).
  void PaintRuler(QPainter* painter) const;
  void PaintShots(QPainter* painter, double y) const;
  void PaintBars(QPainter* painter) const;
  void PaintBar(QPainter* painter, const Bar& bar, bool is_picked) const;
  void PaintLight(QPainter* painter) const;
  void PaintDrag(QPainter* painter) const;
  // The clip shot's effects are drawn over, its first on the reel;
  // nullptr for none.
  const Clip* ClipOf(ShotId shot) const;
  // Every effect's clip, shot by shot (ui/comp_timeline_bars.cpp).
  std::vector<Bar> Bars() const;
  std::optional<Bar> BarAt(QPointF at) const;
  // Reel frames a dragged edge snaps to: shot cuts, unpicked effect
  // clips, markers and the playhead.
  std::vector<int> SnapTargets() const;
  // The nearest target within a few pixels of frame, if any.
  std::optional<int> SnapOf(int frame) const;
  // Drags (ui/comp_timeline_bars.cpp).
  void StartDrag(const Bar& bar, QPointF at);
  void DragTo(QPointF at, bool is_free);
  void MovePicked(int delta, bool is_free);
  // delta kept so every clip in spans stays on the reel, frame 0 to its
  // end.
  int KeptOnReel(const std::map<CompClipRef, Span>& spans, int delta) const;
  void TrimGrabbed(int delta, bool is_free);
  void EndDrag();
  // The input that is not a drag (ui/comp_timeline_input.cpp).
  void PressBar(const Bar& bar, QPointF at, Qt::KeyboardModifiers mods);
  void FinishBox(Qt::KeyboardModifiers mods);
  // Moves every picked clip by delta frames as one undo step.
  void Nudge(int delta);
  void FitPicked();
  void FillMenu(QMenu* menu) const;
  void RunMenu(MenuCommand command);
  Frame Playhead() const;
  void Report(const QString& problem);

  Managers managers_;
  // Zoom and scroll, shared with the Pose and Video timelines; it starts
  // showing the whole reel.
  TimelineScale scale_;
  // The playhead overshoots a little as it lands on a frame.
  PlayheadSettle settle_;
  Grab grab_ = Grab::kNone;
  CompClipRef grabbed_{};
  int press_frame_ = 0;
  QPointF press_at_;
  bool has_moved_ = false;
  // A plain press on a picked clip: a release without a drag narrows
  // the pick to it.
  bool narrows_on_release_ = false;
  std::map<CompClipRef, Span> from_;
  QPointF box_to_;
  std::optional<int> snap_;
  std::unique_ptr<EditScope> drag_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_TIMELINE_H_
