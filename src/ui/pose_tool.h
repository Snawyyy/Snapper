#ifndef SNAPPER_UI_POSE_TOOL_H_
#define SNAPPER_UI_POSE_TOOL_H_

#include <QPointF>
#include <QRectF>
#include <QString>

#include <memory>
#include <optional>
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
// Dragging the lean handle down leans a doll toward the camera this
// many degrees a pixel; up leans it away. There is no end to it.
constexpr double kLeanPerPixel = 0.25;

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
// handle of a doll picked whole tips every whole doll picked toward the
// camera (down) or away (up). A drag is one undo
// step and Escape (Cancel) undoes it on the spot.
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

  bool IsDragging() const { return drag_ != nullptr; }
  // The pick box being drawn, in widget pixels; empty when none.
  QRectF Box() const;
  // The last thing that went wrong, for the status bar; empty if none.
  const QString& problem() const { return problem_; }

 private:
  enum class Kind { kMove, kScale, kIk, kBox, kLean };

  struct Drag final {
    Kind kind = Kind::kMove;
    StageFrame frame;
    // For IK drags: the layer and chain.
    LayerId layer;
    QString chain;
    // For lean drags: every doll picked whole.
    std::vector<LayerId> dolls;
    QPointF start;
    // How far the drag had got at the last move, so each move adds
    // only its own part.
    QPointF done;
    double scaled = 1.0;
    // Ctrl-clicking a picked thing takes it out, unless it was dragged.
    std::optional<Pick> toggle;
    PickMode box_mode = PickMode::kReplace;
    QPointF corner;
    std::unique_ptr<EditScope> scope;
  };

  bool PressIk(QPointF point, const StageFrame& frame);
  bool PressLean(QPointF point, const StageFrame& frame);
  void PressPick(const Pick& pick, bool is_shift, bool is_ctrl,
                 const StageFrame& frame, QPointF point);
  void MoveAll(QPointF total);
  void ScaleAll(double factor);
  void Note(const Result<void>& result);

  Managers managers_;
  ImageCache* cache_;
  std::unique_ptr<Drag> drag_;
  QString problem_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POSE_TOOL_H_
