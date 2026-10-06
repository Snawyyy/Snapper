#ifndef SNAPPER_UI_POSE_TOOL_H_
#define SNAPPER_UI_POSE_TOOL_H_

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "edit/track_ref.h"
#include "model/pose.h"
#include "render/image_cache.h"
#include "ui/managers.h"

namespace snapper {

// Pick handles at least this close, in screen pixels: they look small
// but grab big.
constexpr double kHandleReach = 10.0;
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

// One dot of a picked piece's warp grid.
struct WarpDot final {
  LayerId layer;
  QString piece;
  int point = -1;

  bool operator==(const WarpDot&) const = default;
};

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
// it). A drag is one undo step and Escape (Cancel) undoes it on the
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
  // Double-click: picks the whole doll (or layer) under point as one
  // group, so it moves, scales and turns as one.
  void PickWhole(QPointF point, const StageFrame& frame);
  // The picked warp dot while its piece is still picked and the dot
  // still on its grid.
  std::optional<WarpDot> PickedDot(const StageFrame& frame) const;
  // Escape: drops the picked dot; false when there was none.
  bool DropDot();

  bool IsDragging() const { return drag_ != nullptr; }
  // The pick box being drawn, in widget pixels; empty when none.
  QRectF Box() const;
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
    QPointF start;
    // How far the drag had got at the last move, so each move adds
    // only its own part.
    QPointF done;
    double scaled = 1.0;
    // Ctrl-clicking a picked thing takes it out, unless it was dragged.
    std::optional<Pick> toggle;
    bool has_moved = false;
    PickMode box_mode = PickMode::kReplace;
    QPointF corner;
    std::unique_ptr<EditScope> scope;
  };

  bool PressIk(QPointF point, const StageFrame& frame);
  bool PressLean(QPointF point, const StageFrame& frame);
  bool PressWarp(QPointF point, const StageFrame& frame);
  // The dot of a picked piece's warp grid under point, and that
  // piece's drawing pixels to screen.
  std::optional<std::pair<WarpDot, QTransform>> DotAt(
      QPointF point, const StageFrame& frame) const;
  void WheelReach(const WarpDot& dot, int notches, bool is_fine,
                  const StageFrame& frame);
  void PressPick(const Pick& pick, bool is_shift, bool is_ctrl,
                 const StageFrame& frame, QPointF point);
  void MoveAll(QPointF total);
  void ScaleAll(double factor);
  void Note(const Result<void>& result);

  Managers managers_;
  ImageCache* cache_;
  std::unique_ptr<Drag> drag_;
  std::optional<WarpDot> dot_;
  QString problem_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POSE_TOOL_H_
