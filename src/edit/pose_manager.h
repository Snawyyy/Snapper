#ifndef SNAPPER_EDIT_POSE_MANAGER_H_
#define SNAPPER_EDIT_POSE_MANAGER_H_

#include <QPointF>
#include <QString>

#include <set>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "edit/track_ref.h"
#include "model/pose.h"

namespace snapper {

class HistoryManager;

// A change added to each picked thing's own pose, so things keep their
// differences: x 10 and x 0 moved by 5 become 15 and 5.
// One number of a pose, as the Pose box shows it.
enum class PoseField {
  kTurn, kX, kY, kScaleX, kScaleY, kSkew, kOpacity,
  // Places in front (above 0) or behind (below 0) of its place in the
  // draw order; it holds from key to key, never blends.
  kOrder,
};

struct PoseDelta final {
  double rotation = 0.0;
  QPointF offset;
  double scale_x = 0.0;
  double scale_y = 0.0;
  double skew = 0.0;
  double opacity = 0.0;
  // Draw order places, whole numbers (see PoseField::kOrder).
  double order = 0.0;
};

// Posing at a frame: each call keys the value it changes at that frame
// (keeping the key's ease if one is there, stepping otherwise), so
// posing is just moving things. Values are absolute, not nudges, so a
// drag can call these on every mouse move inside an EditScope.
class PoseManager final {
 public:
  explicit PoseManager(HistoryManager* history);

  // For layer tracks.
  Result<void> Rotate(const TrackRef& track, Frame frame, double degrees);
  Result<void> Move(const TrackRef& track, Frame frame, QPointF offset);
  Result<void> Scale(const TrackRef& track, Frame frame, double x, double y);
  Result<void> Skew(const TrackRef& track, Frame frame, double degrees);
  Result<void> SetOpacity(const TrackRef& track, Frame frame,
                          double opacity);
  Result<void> SetPose(const TrackRef& track, Frame frame, LayerPose pose);
  // Adds delta to every track's pose at frame, as one step. Scale stays
  // 0 or more and opacity 0 to 1.
  Result<void> Shift(const std::vector<TrackRef>& tracks, Frame frame,
                     const PoseDelta& delta);
  // The same at every one of frames: each gets the delta added to what
  // it has, so every frame moves by the same amount from where it was.
  Result<void> Shift(const std::vector<TrackRef>& tracks,
                     const std::vector<Frame>& frames,
                     const PoseDelta& delta);
  // Sets field to value on every track at every one of frames, as one
  // step: a number typed with many frames picked lands on all of them.
  Result<void> SetField(const std::vector<TrackRef>& tracks,
                        const std::vector<Frame>& frames, PoseField field,
                        double value);
  // Moves every track's draw order by `by` places at every one of
  // frames, as one step named for what moved ("Draw order Bo"): a
  // layer moves past other layers.
  Result<void> StepOrder(const std::vector<TrackRef>& tracks,
                         const std::vector<Frame>& frames, int by);
  // The undo name of a draw order change on tracks: "Draw order Bo"
  // for one layer, "Draw order" for several.
  QString OrderName(const std::vector<TrackRef>& tracks) const;
  Result<void> SetCamera(ShotId shot, Frame frame, CameraPose camera);
  // Effect amount tracks, 0 to 1.
  Result<void> SetAmount(const TrackRef& track, Frame frame, double amount);
  // Adds delta to every effect strength at frame, kept 0 to 1.
  Result<void> ShiftAmounts(const std::vector<TrackRef>& tracks, Frame frame,
                            double delta);

  // Keys the layer at frame exactly as it looks there, to mark a pose
  // before changing it.
  Result<void> KeyInPlace(ShotId shot, LayerId layer, Frame frame);


 private:
  // Shift under an undo name of its own.
  Result<void> ShiftAs(const QString& name,
                       const std::vector<TrackRef>& tracks,
                       const std::vector<Frame>& frames,
                       const PoseDelta& delta);

  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_POSE_MANAGER_H_
