#ifndef SNAPPER_UI_POSE_TOOL_H_
#define SNAPPER_UI_POSE_TOOL_H_

#include <QPointF>
#include <QString>

#include <memory>
#include <optional>

#include "base/frame.h"
#include "edit/edit_scope.h"
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

// Where on screen the stage frame is: which shot and frame it shows,
// how big, and where its top-left corner sits in the widget.
struct StageFrame final {
  ShotId shot;
  Frame local;
  double scale = 1.0;
  QPointF corner;
};

// Turns mouse input on the stage into poses. Click picks what is seen;
// drag moves it (Shift locks to one axis, Ctrl scales instead); the
// wheel turns it 5 degrees a notch (1 with Shift); dragging an IK tip
// bends its chain. A drag is one undo step and Escape (Cancel) undoes
// it on the spot.
class PoseTool final {
 public:
  PoseTool(const Managers& managers, ImageCache* cache);

  void Press(QPointF point, bool is_ctrl, const StageFrame& frame);
  void Move(QPointF point, bool is_shift);
  void Release();
  void Cancel();
  void Wheel(int notches, bool is_fine, const StageFrame& frame);

  bool IsDragging() const { return drag_ != nullptr; }
  // The last thing that went wrong, for the status bar; empty if none.
  const QString& problem() const { return problem_; }

 private:
  enum class Kind { kMove, kScale, kIk };

  struct Drag final {
    Kind kind = Kind::kMove;
    StageFrame frame;
    TrackRef track;
    QString chain;
    QPointF start;
    PiecePose start_pose;
    std::unique_ptr<EditScope> scope;
  };

  PiecePose PoseOf(const TrackRef& track, Frame local) const;
  bool PressIk(QPointF point, const StageFrame& frame);
  void Note(const Result<void>& result);

  Managers managers_;
  ImageCache* cache_;
  std::unique_ptr<Drag> drag_;
  QString problem_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_POSE_TOOL_H_
