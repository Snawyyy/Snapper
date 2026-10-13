#ifndef SNAPPER_UI_TIMELINE_VIEW_H_
#define SNAPPER_UI_TIMELINE_VIEW_H_

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QString>
#include <QWidget>

#include <memory>
#include <set>
#include <optional>
#include <vector>

#include "base/frame.h"
#include "edit/change_watcher.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "edit/timeline_rows.h"
#include "model/reel.h"
#include "model/reference.h"
#include "ui/change_effects.h"
#include "ui/drag_feedback.h"
#include "ui/drag_playhead.h"
#include "ui/handle_hover.h"
#include "ui/key_chip_bar.h"
#include "ui/managers.h"
#include "ui/playhead_settle.h"
#include "ui/problem.h"
#include "ui/step_mode_switch.h"
#include "ui/timeline_draft.h"

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
  // The chip, ghost and flash of the key drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }
  // The pops, puffs and rewinds of the last step, for tests to look at.
  const ChangeEffects& effects() const { return effects_; }
  const PlayheadSettle& settle() const { return settle_; }

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void leaveEvent(QEvent* event) override;
  // Files dropped with Alt held become references of the shot.
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  // Reference rows (ui/timeline_references.cpp), one per reference of
  // the shot, between the wave and the first doll row, each with a
  // dashed bar where it plays, its markers as ticks, and an eye. The
  // marks also show as faint lines down the rows below.
  int ReferenceRows() const;
  // Where the doll, layer and camera rows begin.
  double RowsTop() const;
  void PaintReferences(QPainter* painter) const;
  void PaintReferenceMarks(QPainter* painter) const;
  // The reference row under y, or -1.
  int ReferenceRowAt(double y) const;
  // A press on a reference row: its eye shows or hides it; anywhere
  // else picks it, and on its bar starts a drag of its start (Alt:
  // slides the video under the row instead), one undo step. False when
  // the press was not on a reference row.
  bool ReferencePress(const QMouseEvent& event);
  bool ReferenceDragTo(QPointF at);
  bool EndReferenceDrag();
  bool CancelReferenceDrag();
  // Right click on a reference row: find the file, clear markers,
  // remove.
  bool ReferenceMenu(QPoint at, QPoint where);

  void PaintRuler(QPainter* painter) const;
  // Where frames show: right of the names, from top down.
  QRectF FrameArea(double top) const;
  // Faint lines down the rows at each frame's start.
  void PaintGrid(QPainter* painter) const;
  // Frames between ruler numbers at the current zoom.
  int LabelStep() const;
  void PaintWave(QPainter* painter) const;
  // The reel clip the picked shot borrows its sound from; nullptr when
  // it plays the song.
  const Clip* SoundClip() const;
  // A press on the wave band's name: lists the clips of the shot, to
  // pick which one's sound plays under it.
  void PickSoundClip(QPoint where);
  // Names and keys of every row; as a ghost, only the picked keys.
  void PaintRows(QPainter* painter, const std::vector<TimelineRow>& rows,
                 bool is_ghost);
  void PaintKeys(QPainter* painter, const TimelineRow& row,
                 size_t row_index, double middle, bool is_ghost);
  bool IsKeyPicked(const TimelineRow& row, Frame frame) const;
  // As a key is grabbed: leaves a faint copy of the picked keys, drawn
  // by PaintKeys, where they sit now.
  void GrabGhost();
  // Keys just deleted, still shrinking away on their row.
  void PaintPuffs(QPainter* painter, const TimelineRow& row, double middle);
  // The ring around a key that just popped in, when it has one.
  static void PaintRing(QPainter* painter, QPointF center,
                        std::optional<double> progress);
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
  // The mouse moved to frame at at during a key drag: slides the picked
  // keys there once past the dead zone and says how far they went.
  void SlideKeys(Frame frame, QPointF at);
  // The mouse let go: lands the drag as one step and pops.
  void EndKeyDrag();
  // Escape: puts the keys and the pick back. False when no key drag
  // was running.
  bool CancelKeyDrag();
  void Report(const QString& problem) { emit Problem(problem); }
  // Shows the chip bar over the picked keys, or hides it with none in
  // view.
  void ShowChips();
  // Removes the picked keys; the toast offers Undo for that step.
  void DeletePicked();
  void KeyMenu(const TimelineRow& row, Frame frame, QPoint where);
  void EmptyMenu(const TimelineRow& row, Frame frame, QPoint where);

  // A step landed: pops, puffs or rewinds the keys it changed on the
  // picked shot.
  void ShowStep(const Project& before, bool is_travel);

  Managers managers_;
  // Scrub or animation stepping, in the ruler's corner.
  StepModeSwitch step_mode_;
  // Floats above the picked keys.
  KeyChipBar chips_;
  double frame_width_;
  double scroll_ = 0.0;
  bool is_scrubbing_ = false;
  void FinishBox();

  // A box being drawn over keys, and how it combines with the pick.
  bool is_boxing_ = false;
  QPointF box_from_;
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  // A plain press on picked keys: the pick narrows to them on a release
  // that did not drag (pick_keys.h); empty when none waits.
  std::set<KeyRef> later_keys_;
  std::unique_ptr<EditScope> key_drag_;
  // True while a key drag lands its step, so it does not slide again.
  bool is_landing_drag_ = false;
  Frame drag_frame_;
  // The frame the dragged key was pressed on, and the pick then, for
  // Escape to restore.
  Frame drag_origin_;
  std::set<KeyRef> drag_keys_;
  // Where a key was pressed; it slides only once the mouse has gone
  // kDragDeadZone, then jumps under the cursor.
  QPointF press_;
  bool is_started_ = false;
  HandleHover hover_;
  DragFeedback feedback_;
  // Pops, puffs and rewinds of keys the last step made, took away or
  // brought back; they only decorate the paint.
  ChangeWatcher watcher_;
  ChangeEffects effects_;
  // The playhead overshoots a little as it lands on a frame.
  PlayheadSettle settle_;
  // The playhead before the press: a key drag carries it, Escape
  // brings it back.
  DragPlayhead drag_playhead_;
  // A reference bar drag: the step it lands as, which one, whether it
  // slides the video (Alt), and where it and the press started.
  std::unique_ptr<EditScope> reference_drag_;
  ReferenceId reference_dragged_;
  bool is_reference_slide_ = false;
  double reference_press_x_ = 0.0;
  Frame reference_start_;
  double reference_skip_ = 0.0;
  // The Draft row (ui/timeline_draft.cpp), under the reference rows.
  TimelineDraft draft_{managers_, this};
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_VIEW_H_
