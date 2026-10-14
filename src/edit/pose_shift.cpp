// PoseManager: changing many tracks on many frames at once, by a
// difference (Shift) or to one number (SetField).

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/pose_manager.h"

namespace snapper {
namespace {

// Sets one number of pose, kept in range as Shift keeps it; lean and
// swivel only turn layers.
void SetPoseField(PiecePose* pose, PoseField field, double value,
                  bool is_layer) {
  assert(pose != nullptr);
  assert(std::isfinite(value));
  switch (field) {
    case PoseField::kTurn: pose->rotation = value; break;
    case PoseField::kX: pose->offset.setX(value); break;
    case PoseField::kY: pose->offset.setY(value); break;
    case PoseField::kScaleX: pose->scale_x = std::max(0.0, value); break;
    case PoseField::kScaleY: pose->scale_y = std::max(0.0, value); break;
    case PoseField::kSkew: pose->skew = std::clamp(value, -85.0, 85.0); break;
    case PoseField::kOpacity:
      pose->opacity = std::clamp(value, 0.0, 1.0);
      break;
    case PoseField::kLean: pose->lean = is_layer ? value : pose->lean; break;
    case PoseField::kSwivel:
      pose->swivel = is_layer ? value : pose->swivel;
      break;
  }
}

}  // namespace

Result<void> PoseManager::Shift(const std::vector<TrackRef>& tracks,
                                Frame frame, const PoseDelta& delta) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  return Shift(tracks, std::vector<Frame>{frame}, delta);
}

Result<void> PoseManager::Shift(const std::vector<TrackRef>& tracks,
                                const std::vector<Frame>& frames,
                                const PoseDelta& delta) {
  assert(history_ != nullptr);
  assert(frames.size() < 1000000);
  const bool is_finite =
      std::isfinite(delta.rotation) && std::isfinite(delta.offset.x()) &&
      std::isfinite(delta.offset.y()) && std::isfinite(delta.scale_x) &&
      std::isfinite(delta.scale_y) && std::isfinite(delta.skew) &&
      std::isfinite(delta.opacity) && std::isfinite(delta.lean) &&
      std::isfinite(delta.swivel);
  const bool is_usable = is_finite && !tracks.empty() && !frames.empty();
  if (!is_usable) {
    return std::unexpected(Error{Tr("Pick something to change first.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    const bool is_layer = track.kind == TrackKind::kLayer;
    for (const Frame frame : frames) {
      auto shifted = KeyedAt(next, track, frame, PiecePose(),
                             [&delta, is_layer](PiecePose* pose) {
                               pose->rotation += delta.rotation;
                               pose->offset += delta.offset;
                               pose->scale_x = std::max(
                                   0.0, pose->scale_x + delta.scale_x);
                               pose->scale_y = std::max(
                                   0.0, pose->scale_y + delta.scale_y);
                               pose->skew = std::clamp(
                                   pose->skew + delta.skew, -85.0, 85.0);
                               pose->opacity = std::clamp(
                                   pose->opacity + delta.opacity, 0.0, 1.0);
                               pose->lean += is_layer ? delta.lean : 0.0;
                               pose->swivel += is_layer ? delta.swivel : 0.0;
                               return Result<void>();
                             });
      if (!shifted) {
        return std::unexpected(shifted.error());
      }
      next = std::move(*shifted);
    }
  }
  return history_->Apply(Tr("Pose"), std::move(next));
}

Result<void> PoseManager::SetField(const std::vector<TrackRef>& tracks,
                                   const std::vector<Frame>& frames,
                                   PoseField field, double value) {
  assert(history_ != nullptr);
  assert(frames.size() < 1000000);
  const bool is_usable =
      std::isfinite(value) && !tracks.empty() && !frames.empty();
  if (!is_usable) {
    return std::unexpected(Error{Tr("Pick something to change first.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    const bool is_layer = track.kind == TrackKind::kLayer;
    for (const Frame frame : frames) {
      auto set = KeyedAt(next, track, frame, PiecePose(),
                         [field, value, is_layer](PiecePose* pose) {
                           SetPoseField(pose, field, value, is_layer);
                           return Result<void>();
                         });
      if (!set) {
        return std::unexpected(set.error());
      }
      next = std::move(*set);
    }
  }
  return history_->Apply(Tr("Pose"), std::move(next));
}

}  // namespace snapper
