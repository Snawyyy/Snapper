#ifndef SNAPPER_EDIT_POSE_MANAGER_H_
#define SNAPPER_EDIT_POSE_MANAGER_H_

#include <QPointF>
#include <QString>

#include <map>
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
struct PoseDelta final {
  double rotation = 0.0;
  QPointF offset;
  double scale_x = 0.0;
  double scale_y = 0.0;
  double skew = 0.0;
  double opacity = 0.0;
};

// Posing at a frame: each call keys the value it changes at that frame
// (keeping the key's ease if one is there, stepping otherwise), so
// posing is just moving things. Values are absolute, not nudges, so a
// drag can call these on every mouse move inside an EditScope.
class PoseManager final {
 public:
  explicit PoseManager(HistoryManager* history);

  // For layer and piece tracks.
  Result<void> Rotate(const TrackRef& track, Frame frame, double degrees);
  Result<void> Move(const TrackRef& track, Frame frame, QPointF offset);
  Result<void> Scale(const TrackRef& track, Frame frame, double x, double y);
  Result<void> Skew(const TrackRef& track, Frame frame, double degrees);
  Result<void> SetOpacity(const TrackRef& track, Frame frame,
                          double opacity);
  // Piece tracks: show drawing (index into the piece's drawings; -1 for
  // the rig's default).
  Result<void> SwapDrawing(const TrackRef& track, Frame frame, int drawing);
  // Piece tracks: push one warp grid point by offset from rest.
  Result<void> Warp(const TrackRef& track, Frame frame, int point,
                    QPointF offset);
  Result<void> SetPose(const TrackRef& track, Frame frame, PiecePose pose);
  // Adds delta to every track's pose at frame, as one step. Scale stays
  // 0 or more and opacity 0 to 1.
  Result<void> Shift(const std::vector<TrackRef>& tracks, Frame frame,
                     const PoseDelta& delta);
  Result<void> SetCamera(ShotId shot, Frame frame, CameraPose camera);
  // Effect amount tracks, 0 to 1.
  Result<void> SetAmount(const TrackRef& track, Frame frame, double amount);

  // Keys the layer at frame exactly as it looks there (a whole doll for
  // a doll layer), to mark a pose before changing it.
  Result<void> KeyInPlace(ShotId shot, LayerId layer, Frame frame);

  // Bends chain on a doll layer so its tip reaches target (doll space).
  Result<void> DragIk(ShotId shot, LayerId layer, const QString& chain,
                      Frame frame, QPointF target);

  // Copies the poses of pieces (all of them when empty) at frame.
  Result<void> CopyPose(ShotId shot, LayerId layer, Frame frame,
                        const std::set<QString>& pieces);
  // Keys the copied pose at frame, mirrored left to right if asked.
  Result<void> PastePose(ShotId shot, LayerId layer, Frame frame,
                         bool is_mirrored);
  // Why Paste can't act, for its tooltip; empty when it can.
  QString WhyNoPaste() const;

 private:
  HistoryManager* history_;
  // The copied pose, by piece name. Not part of the project: copying
  // is not an edit.
  std::map<QString, PiecePose> clipboard_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_POSE_MANAGER_H_
