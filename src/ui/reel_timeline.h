#ifndef SNAPPER_UI_REEL_TIMELINE_H_
#define SNAPPER_UI_REEL_TIMELINE_H_

#include <QPointF>
#include <QRectF>
#include <QScrollBar>
#include <QString>
#include <QWidget>

class QKeyEvent;
class QMenu;

#include <memory>
#include <set>
#include <vector>

#include "anim/reel_timeline.h"
#include "base/frame.h"
#include "edit/change_watcher.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "model/reel.h"
#include "ui/change_effects.h"
#include "ui/drag_feedback.h"
#include "ui/drag_playhead.h"
#include "ui/handle_hover.h"
#include "base/error.h"
#include "ui/managers.h"
#include "ui/playhead_settle.h"
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
// along or to another track; drag its ends to trim it. The ends of a
// shot clip showing its whole shot change the shot's own length instead
// (Alt flips trim and resize, Shift ripples, ReelManager::ResizeShots);
// double-clicking a shot clip inside a gap offers to fit the shot to it.
// Ctrl turns snapping off. Drags snap to
// other clips' cuts, cut markers and the playhead, and flash when they
// do; a chip beside the cursor says how many frames the drag went, a
// faint ghost marks where it began, and letting go pops. Escape cancels
// one.
// Click or drag the ruler to move the playhead. Cut markers (Mark cut
// on the Video tab) show as yellow notches and dashed lines;
// double-click a track between two of them to pick which part of a
// video plays in that gap (SlotPicker). Double-click the cut between
// two touching clips to pick its transition (TransitionPicker).
// Keys: S splits at the playhead (the picked clips, or with none
// picked what the playhead is in), Delete removes, Shift+Delete
// removes and closes the gap, Ctrl+C, Ctrl+X and Ctrl+V copy, cut and
// paste at the playhead, Ctrl+D duplicates; right-click for the same
// and for tracks. The wheel scrolls time, Ctrl zooms; Shift, or the
// wheel over the track names, scrolls the tracks up and down (a scroll
// bar at the right shows when they do not all fit). Shots here are
// finished videos: what is inside them is edited on the Pose tab;
// double-clicking a shot clip asks to open it there (OpenShotAsked).
// Alt and a drag over empty track space marks a range; "New shot here"
// (the empty track's double click menu, the track menu, the shot bin's
// "+" tile) makes a shot sized to it, else to the gap there.
// Sound tracks sit at the bottom: sound files drop there (or anywhere,
// landing on the first free one), each clip shows its waveform, a
// volume line to drag and fade handles; the header has Mute, Solo, R
// (arm for recording) and a fader.
class ReelTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelTimeline(const Managers& managers);

  // Where clip is drawn; empty when it is gone.
  QRectF ClipRect(ClipId clip) const;
  // The clip whose end at is on, where the next clip on its track
  // starts: the cut a transition lives on. Invalid elsewhere.
  ClipId JointAt(QPointF at) const;
  // The clip whose cut has its badge under at; invalid for none. A
  // click on a badge opens that transition.
  ClipId BadgeAt(QPointF at) const;
  // The track drawn at y, or -1 off the tracks.
  int TrackAt(double y) const;
  // The reel frame at x, never before 0.
  Frame FrameAt(double x) const;
  double XOf(Frame frame) const;
  double TrackTop(int track) const;
  // Pixels per frame.
  double zoom() const { return zoom_; }
  void SetZoom(double pixels_per_frame);
  // The chip, ghost and snap flash of the drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }
  // The pops, puffs and rewinds of the last step, for tests to look at.
  const ChangeEffects& effects() const { return effects_; }
  const PlayheadSettle& settle() const { return settle_; }
  // The range marked with Alt and a drag on a picture track; invalid
  // when none is.
  Slot marked() const { return marked_; }
  int marked_track() const { return marked_track_; }
  // New shot here over the marked range, else the gap at the playhead,
  // on the lowest picture track with room. Picks the new clip.
  void NewShotAtPlayhead();

 signals:
  void Problem(const QString& why);
  // The shot edge mode was switched from a menu.
  void EdgeModeChanged();
  // A text clip was double-clicked: its words want typing.
  void TextEditAsked(ClipId clip);
  // A shot clip was double-clicked: open shot on the Pose tab at frame
  // (counted in the shot) under the click.
  void OpenShotAsked(ShotId shot, Frame frame);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  // kVolume, kFadeIn and kFadeOut drag a sound clip's volume line and
  // fade handles; kFader a sound track header's volume.
  enum class Drag {
    kNone,
    kSeek,
    kMove,
    kTrimStart,
    kTrimEnd,
    kBox,
    kMark,
    kVolume,
    kFadeIn,
    kFadeOut,
    kFader,
  };
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
  void PaintPictureBody(QPainter* painter, const Clip& clip,
                        const QRectF& box) const;
  void PaintHatch(QPainter* painter, const Clip& clip,
                  const QRectF& box) const;
  // A text clip or cue: its first line, a swatch of its fill, and a
  // warning on a cue that wraps past two lines.
  void PaintTextBody(QPainter* painter, const Clip& clip,
                     const QRectF& box) const;

  // Sound (ui/reel_timeline_sound.cpp). A sound clip: its waveform
  // (dimmed on a track not heard), volume line and fade handles.
  void PaintSoundBody(QPainter* painter, const Clip& clip,
                      const QRectF& box) const;
  // A sound track's header: its name, Mute and Solo, and a fader.
  void PaintSoundHeader(QPainter* painter, int track) const;
  QRectF MuteRect(int track) const;
  QRectF SoloRect(int track) const;
  QRectF FaderRect(int track) const;
  // R: arms the sound track for recording.
  QRectF ArmRect(int track) const;
  // Where a sound clip's volume line is drawn in box.
  static double VolumeY(const QRectF& box, double gain);
  // The volume line or a fade handle of a sound clip under at, or kNone.
  Drag SoundDragFor(ClipId clip, QPointF at) const;
  // A press on a sound track's header: Mute, Solo, or the fader (which
  // starts a drag). False when at is on none of them.
  bool PressSoundHeader(QPointF at);
  // A double click on a fader or volume line puts it back to 0 dB.
  bool ResetSoundAt(QPointF at);
  // Opens the drag's step for a volume line or fade handle drag.
  void StartSoundDrag(ClipId clip, Drag drag, QPointF at);
  // Follows a volume line, fade or fader drag; the chip shows the value.
  void FollowSound(QPointF at);
  // "V2" for a picture track; a sound track's name, or "A1".
  QString TrackName(int track) const;
  // Mute and Solo in a sound track's menu; nothing for other tracks.
  void AddSoundTrackEntries(QMenu* menu, int track);
  // Picking one shot clip makes it the one its shot borrows its sound
  // from on the Pose tab.
  void PreferPicked();
  // PaintClip, grown, flashed or ringed while the clip pops in or is
  // brought back by undo or redo.
  void PaintClipFx(QPainter* painter, const Clip& clip) const;
  // How far to draw clip (on track) from its place while it slides.
  QPointF SlideShift(const Clip& clip, int track) const;
  // Clips just deleted, still shrinking away where they were.
  void PaintPuffs(QPainter* painter) const;
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
  // An end drag of a shot clip resizes its shot when ReelManager says so
  // for the modifiers held at the press (Alt flips it).
  void StartDrag(ClipId clip, Drag drag, QPointF at,
                 Qt::KeyboardModifiers modifiers);
  // As a clip or its end is grabbed: leaves a faint copy of the clips
  // that drag, drawn by PaintClip, where they sit now.
  void GrabGhost();
  // Ctrl turns snapping off; Shift ripples a shot resize.
  void Follow(QPointF at, Qt::KeyboardModifiers modifiers);
  // A shot clip end drag that changes the shot's length; the chip says
  // the new length, the change and what stopped it.
  void FollowResize(Frame raw, Frame edge, QPointF at, bool is_ripple);
  // Moves the playhead to the frame the dragged clip edge lands on.
  void CarryPlayhead(Frame landing);
  // Shows a trim: how many frames the edge went from its start, and a
  // flash when it snapped from the frame under the cursor (raw).
  void ShowTrim(Frame raw, Frame edge, QPointF at);
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
  // The edge mode entries of a menu: Hold and Stretch.
  void AddEdgeModes(QMenu* menu);
  // A double click on empty picture track space: New shot here (sized
  // to the marked range when at is in it, else to the gap) and, inside
  // a gap between markers, what plays there.
  void OfferEmpty(int track, Frame at, QPoint where);
  // New shot here on track at at, sized as OfferEmpty says; picks it.
  void NewShotHere(int track, Frame at);
  // The stretch New shot here at at on track fills.
  Slot NewShotSpanAt(int track, Frame at) const;
  // Fit shot to gap entries for clip at at, when it can be fitted.
  void AddFitEntries(QMenu* menu, ClipId clip, Frame at);
  // The tracks' full height and the pixels scrolled off the top; the
  // scroll bar shows only when the tracks do not fit.
  double TracksHeight() const;
  void FitScrollBar();
  // The marked range, a band over its track and the ruler.
  void PaintMarked(QPainter* painter) const;
  void ClipMenu(QPoint where);
  void TrackMenu(int track, QPoint where);
  void SeekTo(double x);
  // How many frames of snapping reach the zoom gives.
  int SnapReach() const;
  // The name the hover knows a clip start or end edge by.
  static QString EdgeKey(ClipId clip, bool is_start);
  void Report(const QString& problem) { emit Problem(problem); }
  // A step landed: pops, puffs or rewinds the clips it changed.
  void ShowStep(const Project& before, bool is_travel);
  // Passes on a delete's outcome; the toast offers Undo for that step.
  void DeletePicked(const Result<void>& result, size_t count);

  Managers managers_;
  // The clip edge under the cursor widens and brightens.
  HandleHover hover_;
  DragFeedback feedback_;
  // Decoration only; the clips are in the project at once.
  ChangeWatcher watcher_;
  ChangeEffects effects_;
  // The playhead overshoots a little as it lands on a frame.
  PlayheadSettle settle_;
  // The playhead before a drag: the drag carries it, Escape returns it.
  DragPlayhead drag_playhead_;
  double zoom_;
  // Frames scrolled off the left.
  double scroll_ = 0.0;
  Drag drag_ = Drag::kNone;
  ClipId dragged_;
  // The sound track whose fader drags.
  int drag_track_ = -1;
  QPointF press_;
  // A plain click on a clip already in a bigger pick picks just it, once
  // it is clear the click was not a drag.
  bool is_plain_press_ = false;
  // True while a drag lands its step, so its clips do not slide again.
  bool is_landing_drag_ = false;
  bool has_moved_ = false;
  // The end drag changes the shot's length instead of trimming.
  bool is_resize_ = false;
  // An Alt drag of a cue's edge: the touching cue's edge goes along.
  bool is_roll_ = false;
  // The rubber band while picking by box, and how it combines.
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  std::unique_ptr<EditScope> scope_;
  // Where a dragged shot or file would land, while one hovers.
  int drop_track_ = -1;
  Frame drop_frame_;
  Frame drop_length_;
  // Alt and a drag over empty space marks a range for New shot here.
  Slot marked_;
  int marked_track_ = -1;
  // Tracks scrolled up and down; its value is the pixels off the top.
  QScrollBar track_scroll_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_TIMELINE_H_
