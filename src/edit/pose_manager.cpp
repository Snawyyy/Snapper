#include "edit/pose_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <variant>

#include "edit/history_manager.h"
#include "edit/pose_edits.h"

namespace snapper {
PoseManager::PoseManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(history_->current().shots.size() <= static_cast<size_t>(kMaxShots));
}

Result<void> PoseManager::SetPose(const TrackRef& track, Frame frame,
                                  LayerPose pose) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  return history_->Apply(
      Tr("Pose"), KeyedAt(history_->current(), track, frame, LayerPose(),
                          [&pose](LayerPose* value) {
                            *value = pose;
                            return Result<void>();
                          }));
}

Result<void> PoseManager::Rotate(const TrackRef& track, Frame frame,
                                 double degrees) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_finite = std::isfinite(degrees);
  if (!is_finite) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  return history_->Apply(
      Tr("Rotate"), KeyedAt(history_->current(), track, frame, LayerPose(),
                            [degrees](LayerPose* value) {
                              value->rotation = degrees;
                              return Result<void>();
                            }));
}

Result<void> PoseManager::Move(const TrackRef& track, Frame frame,
                               QPointF offset) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_finite = std::isfinite(offset.x()) &&
                         std::isfinite(offset.y());
  if (!is_finite) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  return history_->Apply(
      Tr("Move"), KeyedAt(history_->current(), track, frame, LayerPose(),
                          [offset](LayerPose* value) {
                            value->offset = offset;
                            return Result<void>();
                          }));
}

Result<void> PoseManager::Scale(const TrackRef& track, Frame frame, double x,
                                double y) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_finite = std::isfinite(x) && std::isfinite(y);
  if (!is_finite) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  return history_->Apply(
      Tr("Scale"), KeyedAt(history_->current(), track, frame, LayerPose(),
                           [x, y](LayerPose* value) {
                             value->scale_x = x;
                             value->scale_y = y;
                             return Result<void>();
                           }));
}

Result<void> PoseManager::Skew(const TrackRef& track, Frame frame,
                               double degrees) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_usable = std::isfinite(degrees) && std::abs(degrees) < 89.0;
  if (!is_usable) {
    return std::unexpected(Error{Tr("Skew stays between -89 and 89.")});
  }
  return history_->Apply(
      Tr("Skew"), KeyedAt(history_->current(), track, frame, LayerPose(),
                          [degrees](LayerPose* value) {
                            value->skew = degrees;
                            return Result<void>();
                          }));
}

Result<void> PoseManager::SetOpacity(const TrackRef& track, Frame frame,
                                     double opacity) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_usable = opacity >= 0.0 && opacity <= 1.0;
  if (!is_usable) {
    return std::unexpected(Error{Tr("Opacity runs from 0 to 1.")});
  }
  return history_->Apply(
      Tr("Fade"), KeyedAt(history_->current(), track, frame, LayerPose(),
                          [opacity](LayerPose* value) {
                            value->opacity = opacity;
                            return Result<void>();
                          }));
}

Result<void> PoseManager::KeyInPlace(ShotId shot, LayerId layer,
                                     Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const TrackRef track{shot, TrackKind::kLayer, layer};
  return history_->Apply(Tr("Add key"),
                         KeyedAt(history_->current(), track, frame,
                                 LayerPose(), [](LayerPose*) {
                                   return Result<void>();
                                 }));
}

Result<void> PoseManager::SetCamera(ShotId shot, Frame frame,
                                    CameraPose camera) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_usable = std::isfinite(camera.center.x()) &&
                         std::isfinite(camera.center.y()) &&
                         camera.zoom > 0.0 &&
                         std::isfinite(camera.zoom) &&
                         std::isfinite(camera.rotation) &&
                         camera.shake >= 0.0;
  if (!is_usable) {
    return std::unexpected(
        Error{Tr("Zoom must be above zero and shake can't be negative.")});
  }
  const TrackRef track{shot, TrackKind::kCamera, {}};
  return history_->Apply(
      Tr("Move camera"),
      KeyedAt(history_->current(), track, frame, CameraPose(),
              [&camera](CameraPose* value) {
                *value = camera;
                return Result<void>();
              }));
}

Result<void> PoseManager::ShiftAmounts(const std::vector<TrackRef>& tracks,
                                       Frame frame, double delta) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_usable = std::isfinite(delta) && !tracks.empty();
  if (!is_usable) {
    return std::unexpected(Error{Tr("Pick an effect first.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    auto shifted = KeyedAt(next, track, frame, 1.0, [delta](double* amount) {
      *amount = std::clamp(*amount + delta, 0.0, 1.0);
      return Result<void>();
    });
    if (!shifted) {
      return std::unexpected(shifted.error());
    }
    next = std::move(*shifted);
  }
  return history_->Apply(Tr("Effect strength"), std::move(next));
}

Result<void> PoseManager::SetAmount(const TrackRef& track, Frame frame,
                                    double amount) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_usable = amount >= 0.0 && amount <= 1.0;
  if (!is_usable) {
    return std::unexpected(Error{Tr("Strength runs from 0 to 1.")});
  }
  return history_->Apply(
      Tr("Effect strength"),
      KeyedAt(history_->current(), track, frame, 1.0,
              [amount](double* value) {
                *value = amount;
                return Result<void>();
              }));
}

}  // namespace snapper
