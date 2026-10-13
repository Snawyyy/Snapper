#ifndef SNAPPER_UI_POSE_TOOL_H_
#define SNAPPER_UI_POSE_TOOL_H_

#include <Qt>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "anim/guides.h"
#include "base/frame.h"
#include "edit/edit_scope.h"
#include "edit/rig_manager.h"
#include "edit/selection_manager.h"
#include "edit/track_ref.h"
#include "model/pose.h"
#include "render/image_cache.h"
#include "ui/grab.h"
#include "ui/managers.h"

namespace snapper {

constexpr double kWheelStep = 5.0;
constexpr double kFineWheelStep = 1.0;
// Dragging this many pixels doubles (or halves) the size.
constexpr double kScalePixels = 200.0;
// Dragging a turn handle turns a doll this many degrees a pixel: the
// lean handle down leans it toward the camera, the swivel handle right
// brings its right side toward the camera. There is no end to either.
constexpr double kTurnPerPixel = 0.25;

// Each wheel notch on a picked warp dot changes its rubber reach by this
// many grid cells (the fine step with Shift).
constexpr double kReachStep = 0.5;
constexpr double kFineReachStep = 0.1;

// Where on screen the stage frame is: which shot and frame it shows,
// how big, and where its top-left corner sits in the widget.
struct StageFrame final {
  ShotId shot;
  Frame local;
  double scale = 1.0;
  QPointF corner;
};

// Turns mouse input on the stage into poses. Click picks what is seen
// (Shift adds, Ctrl flips in or out); dragging empty space draws a box
// that picks what it touches (Shift adds, Ctrl takes out). Dragging a
// pick moves everything picked by the same amount (Shift locks to one
// axis, Ctrl scales instead); the wheel turns them 5 degrees a notch
// (1 with Shift); dragging an IK tip bends its chain; dragging the lean
// or swivel handle of a doll picked whole turns every whole doll picked
// in depth; dragging a dot of a picked piece's warp grid bends the
// drawing there, and clicking one picks it so the wheel sets how far it
// pulls its neighbours (clicking it again, or anything else, drops
// it). Shift on a turn handle snaps the turn to 15 degrees. A drag is
// one undo step and Escape (Cancel) undoes it on the
// spot.
class PoseTool final {
 public:
  PoseTool(const Managers& managers, ImageCache* cache);

  void Press(QPointF point, bool is_shift, bool is_ctrl,
             const StageFrame& frame);
  void Move(QPointF point, bool is_shift);
  void Release();
  void Cancel();
  void Wheel(int notches, bool is_fine, const StageFrame& frame);
  // An arrow key: moves everything picked by delta frame pixels as one
  // undo step. Nothing while a drag runs or with nothing picked.
  void Nudge(QPointF delta, const StageFrame& frame);
  // The degrees a wheel turn of notches turns the picked things by.
  static double WheelDegrees(int notches, bool is_fine);
  // Double-click: picks the whole doll (or layer) under point as one
  // group, so it moves, scales and turns as one.
  void PickWhole(QPointF point, const StageFrame& frame);
  // The picked warp dot while its piece is still picked and the dot
  // still on its grid.
  std::optional<WarpDot> PickedDot(const StageFrame& frame) const;
  // Every picked warp dot, while its piece is still picked.
  std::vector<WarpDot> PickedDots(const StageFrame& frame) const;
  // The warp dots of picked pieces that lie inside box (widget pixels).
  std::set<WarpDot> DotsIn(const QRectF& box, const StageFrame& frame) const;
  // Escape: drops the picked dots; false when there were none.
  bool DropDot();
  // D: makes the picked dots drag behind, or stop dragging when they
  // all do already.
  void ToggleDrag(const StageFrame& frame);
  // Delete: takes the drag or physics node and the motion off every
  // picked dot; false when none is picked.
  bool ClearDots(const StageFrame& frame);

  // What is under the pointer before any press: the handle (kNone when
  // none) and, when no handle, the unpicked thing a click would pick.
  struct Hover final {
    GrabKind kind = GrabKind::kNone;
    QString key;
    std::optional<Pick> pick;
  };
  Hover HoverAt(QPointF point, const StageFrame& frame) const;
  // The selection changed: notes whether handles are on show, so a
  // press just after they hide still lands.
  void Observe();
  // The names a handle is known by, for hovering and drawing it.
  static QString WarpKey(const WarpDot& dot);
  static QString TurnKey(LayerId layer);
  static QString IkKey(const QString& chain);

  // What a drag in progress should say: the live value in plain words
  // (a turn snapped by Shift says so), the point it began and the
  // cursor. Empty until the drag has left the dead zone, and for the
  // pick box.
  struct Readout final {
    QString text;
    bool is_snapped = false;
    QPointF start;
    QPointF cursor;
  };
  std::optional<Readout> ReadoutOf() const;

  bool IsDragging() const { return drag_ != nullptr; }
  // True while the drag is drawing the pick box, not moving anything.
  bool IsBoxing() const {
    return drag_ != nullptr && drag_->kind == Kind::kBox;
  }
  // The pick box being drawn, in widget pixels; empty when none.
  QRectF Box() const;
  // The lines a move drag has caught on, in widget pixels; none when it
  // has caught nothing (or no move runs).
  const std::vector<GuideLine>& guides() const { return guides_; }
  // Whether a move drag catches on guides (it does at first).
  void SetGuides(bool is_on) { is_catching_ = is_on; }
  // The last thing that went wrong, for the status bar; empty if none.
  const QString& problem() const { return problem_; }

 private:
  enum class Kind { kMove, kScale, kIk, kBox, kLean, kSwivel, kWarp };

  struct Drag final {
    Kind kind = Kind::kMove;
    StageFrame frame;
    // For IK drags: the layer and chain.
    LayerId layer;
    QString chain;
    // For warp drags: the piece, its grid point, and drawing pixels to
    // screen.
    QString piece;
    int point = -1;
    QTransform drawing_to_screen;
    // For lean and swivel drags: every doll picked whole.
    std::vector<LayerId> dolls;
    // Where the mouse went down, for the dead zone; the drag measures
    // from start, which is a handle's own spot when it is one.
    QPointF press;
    QPointF start;
    // How far the drag had got at the last move, so each move adds
    // only its own part.
    QPointF done;
    double scaled = 1.0;
    // A pick a release that did not drag applies with later_mode:
    // Ctrl-clicking a picked thing takes it out, a plain click on a
    // picked thing narrows the pick to it (pick_keys.h).
    std::optional<Pick> later;
    PickMode later_mode = PickMode::kReplace;
    // True once the mouse has left the dead zone.
    bool has_moved = false;
    // What the drag says at the last move; empty before the first.
    Readout readout;
    PickMode box_mode = PickMode::kReplace;
    QPointF corner;
    // Move drags: the picked things' box when pressed, and the boxes
    // they may line up with, in widget pixels.
    QRectF moving;
    std::vector<QRectF> targets;
    // Lines it already lined up with at the press: it does not stick
    // to those.
    std::vector<GuideLine> skip;
    std::unique_ptr<EditScope> scope;
  };

  // A handle on the stage: where it sits and what grabbing it needs.
  struct Handle final {
    GrabKind kind = GrabKind::kNone;
    QString key;
    // Widget pixels.
    QPointF spot;
    // Warp dots: which, and its drawing pixels to screen.
    WarpDot dot;
    QTransform to_screen;
    // IK tips: layer and chain.
    LayerId layer;
    QString chain;
  };

  // The picks whose handles are grabbable now: the live ones, or the
  // ones that hid within kGraceMs.
  std::vector<DotRef> PickedRefs(const StageFrame& frame) const;
  std::set<Pick> GrabbablePicks(const StageFrame& frame) const;
  std::vector<Handle> HandlesOf(const StageFrame& frame) const;
  std::optional<Handle> HandleAt(QPointF point,
                                 const StageFrame& frame) const;
  bool PressHandle(QPointF point, const StageFrame& frame);
  void GrabTurn(const Handle& handle, QPointF point,
                const StageFrame& frame);
  void GrabWarp(const Handle& handle, QPointF point,
                const StageFrame& frame);
  void GrabIk(const Handle& handle, QPointF point,
              const StageFrame& frame);
  int64_t NowMs() const { return clock_.elapsed(); }
  void WheelReach(int notches, bool is_fine, const StageFrame& frame);
  void PressPick(const Pick& pick, Qt::KeyboardModifiers mods,
                 const StageFrame& frame, QPointF point);
  // True once a drag may change things: the mouse has left the dead
  // zone (a pick box has none).
  bool LeftDeadZone(QPointF point);
  // Turns every doll of the drag by the mouse's pixels, snapped to
  // kTurnSnapDegrees with Shift, and notes what it says.
  void MoveTurn(QPointF total, QPointF point, bool is_swivel,
                bool is_shift);
  // Notes what the drag says now, with the cursor at point.
  void Say(const QString& text, bool is_snapped, QPointF point);
  void MoveIk(QPointF point);
  void MoveAll(QPointF total);
  // The frames an edit lands on: those of the keys picked on the
  // timeline in at's shot (so a move reaches them all, each moved by the
  // same amount from where it was), or just the playhead's with none
  // picked.
  std::vector<Frame> EditFrames(const StageFrame& at) const;
  // As a move drag starts: notes the box of what is picked and the boxes
  // it may line up with (the other things, the frame).
  void ArmGuides(const StageFrame& frame);
  // total moved to catch on a guide, noting the lines caught.
  QPointF Snapped(QPointF total);
  void ScaleAll(double factor);
  void Note(const Result<void>& result);

  Managers managers_;
  ImageCache* cache_;
  std::unique_ptr<Drag> drag_;
  std::vector<GuideLine> guides_;
  bool is_catching_ = true;
  QString problem_;
  QElapsedTimer clock_;
  HandleGrace grace_;
  // The picks, layer and shot whose handles were last on show.
  std::set<Pick> shown_picks_;
  LayerId shown_layer_;
  ShotId shown_shot_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POSE_TOOL_H_
