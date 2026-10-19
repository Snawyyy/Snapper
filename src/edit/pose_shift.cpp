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
#include "model/shot.h"

namespace snapper {
namespace {

// A draw order stays within as many places as a shot has layers, as
// the project file keeps it.
int ClampedOrder(double places) {
  assert(std::isfinite(places));
  assert(kMaxLayersPerShot > 0);
  return static_cast<int>(std::clamp(std::lround(places),
                                     static_cast<long>(-kMaxLayersPerShot),
                                     static_cast<long>(kMaxLayersPerShot)));
}

// Sets one number of pose, kept in range as Shift keeps it.
void SetPoseField(LayerPose* pose, PoseField field, double value) {
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
    case PoseField::kOrder: pose->order = ClampedOrder(value); break;
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
  return ShiftAs(Tr("Pose"), tracks, frames, delta);
}

Result<void> PoseManager::ShiftAs(const QString& name,
                                  const std::vector<TrackRef>& tracks,
                                  const std::vector<Frame>& frames,
                                  const PoseDelta& delta) {
  assert(history_ != nullptr);
  assert(!name.isEmpty());
  const bool is_finite =
      std::isfinite(delta.rotation) && std::isfinite(delta.offset.x()) &&
      std::isfinite(delta.offset.y()) && std::isfinite(delta.scale_x) &&
      std::isfinite(delta.scale_y) && std::isfinite(delta.skew) &&
      std::isfinite(delta.opacity) && std::isfinite(delta.order);
  const bool is_usable = is_finite && !tracks.empty() && !frames.empty();
  if (!is_usable) {
    return std::unexpected(Error{Tr("Pick something to change first.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    for (const Frame frame : frames) {
      auto shifted = KeyedAt(next, track, frame, LayerPose(),
                             [&delta](LayerPose* pose) {
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
                               pose->order = ClampedOrder(pose->order +
                                                          delta.order);
                               return Result<void>();
                             });
      if (!shifted) {
        return std::unexpected(shifted.error());
      }
      next = std::move(*shifted);
    }
  }
  return history_->Apply(name, std::move(next));
}

Result<void> PoseManager::StepOrder(const std::vector<TrackRef>& tracks,
                                    const std::vector<Frame>& frames,
                                    int by) {
  assert(history_ != nullptr);
  assert(by >= -kMaxLayersPerShot && by <= kMaxLayersPerShot);
  PoseDelta delta;
  delta.order = by;
  return ShiftAs(OrderName(tracks), tracks, frames, delta);
}

QString PoseManager::OrderName(const std::vector<TrackRef>& tracks) const {
  assert(history_ != nullptr);
  assert(tracks.size() < 100000);
  const bool is_one = tracks.size() == 1;
  if (!is_one) {
    return Tr("Draw order");
  }
  const TrackRef& track = tracks.front();
  const Shot* shot = FindShot(history_->current(), track.shot);
  const Layer* layer = shot != nullptr ? FindLayer(*shot, track.layer)
                                       : nullptr;
  const QString name = layer != nullptr ? layer->name : QString();
  const bool is_named = !name.isEmpty();
  return is_named ? Tr("Draw order %1").arg(name) : Tr("Draw order");
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
    for (const Frame frame : frames) {
      auto set = KeyedAt(next, track, frame, LayerPose(),
                         [field, value](LayerPose* pose) {
                           SetPoseField(pose, field, value);
                           return Result<void>();
                         });
      if (!set) {
        return std::unexpected(set.error());
      }
      next = std::move(*set);
    }
  }
  const bool is_order = field == PoseField::kOrder;
  return history_->Apply(is_order ? OrderName(tracks) : Tr("Pose"),
                         std::move(next));
}

}  // namespace snapper
