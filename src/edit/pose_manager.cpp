#include "edit/pose_manager.h"

#include <cassert>
#include <cmath>
#include <utility>
#include <variant>

#include "edit/history_manager.h"
#include "edit/pose_edits.h"

namespace snapper {
namespace {

// The rig and art of the piece a piece track moves; nullptrs when the
// track's layer, doll or piece is gone.
std::pair<const RigPiece*, const ArtPiece*> PieceOf(const Project& project,
                                                    const TrackRef& track) {
  assert(track.kind == TrackKind::kPiece);
  assert(!track.piece.isEmpty());
  const Doll* doll = DollOfLayer(project, track.shot, track.layer);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return {nullptr, nullptr};
  }
  return {FindRig(doll->rig, track.piece), FindArt(*doll, track.piece)};
}

}  // namespace

PoseManager::PoseManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(clipboard_.empty());
}

Result<void> PoseManager::SetPose(const TrackRef& track, Frame frame,
                                  PiecePose pose) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  return history_->Apply(
      Tr("Pose"), KeyedAt(history_->current(), track, frame, PiecePose(),
                          [&pose](PiecePose* value) {
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
      Tr("Rotate"), KeyedAt(history_->current(), track, frame, PiecePose(),
                            [degrees](PiecePose* value) {
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
      Tr("Move"), KeyedAt(history_->current(), track, frame, PiecePose(),
                          [offset](PiecePose* value) {
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
      Tr("Scale"), KeyedAt(history_->current(), track, frame, PiecePose(),
                           [x, y](PiecePose* value) {
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
      Tr("Skew"), KeyedAt(history_->current(), track, frame, PiecePose(),
                          [degrees](PiecePose* value) {
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
      Tr("Fade"), KeyedAt(history_->current(), track, frame, PiecePose(),
                          [opacity](PiecePose* value) {
                            value->opacity = opacity;
                            return Result<void>();
                          }));
}

Result<void> PoseManager::SwapDrawing(const TrackRef& track, Frame frame,
                                      int drawing) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_piece = track.kind == TrackKind::kPiece;
  const ArtPiece* art =
      is_piece ? PieceOf(history_->current(), track).second : nullptr;
  const bool is_valid =
      art != nullptr && drawing >= -1 &&
      drawing < static_cast<int>(art->drawings.size());
  if (!is_valid) {
    return std::unexpected(Error{Tr("That piece has no such drawing.")});
  }
  return history_->Apply(
      Tr("Swap %1").arg(track.piece),
      KeyedAt(history_->current(), track, frame, PiecePose(),
              [drawing](PiecePose* value) {
                value->drawing = drawing;
                return Result<void>();
              }));
}

Result<void> PoseManager::Warp(const TrackRef& track, Frame frame, int point,
                               QPointF offset) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_piece = track.kind == TrackKind::kPiece;
  const RigPiece* rig =
      is_piece ? PieceOf(history_->current(), track).first : nullptr;
  const int count = rig != nullptr ? rig->warp.PointCount() : 0;
  const bool is_valid = point >= 0 && point < count &&
                        std::isfinite(offset.x()) &&
                        std::isfinite(offset.y());
  if (!is_valid) {
    return std::unexpected(
        Error{Tr("Give the piece a warp grid in the rig first.")});
  }
  return history_->Apply(
      Tr("Warp %1").arg(track.piece),
      KeyedAt(history_->current(), track, frame, PiecePose(),
              [count, point, offset](PiecePose* value) {
                const bool is_fitting =
                    static_cast<int>(value->warp.size()) == count;
                if (!is_fitting) {
                  value->warp.assign(static_cast<size_t>(count), QPointF());
                }
                value->warp[static_cast<size_t>(point)] = offset;
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
  const TrackRef track{shot, TrackKind::kCamera, {}, {}};
  return history_->Apply(
      Tr("Move camera"),
      KeyedAt(history_->current(), track, frame, CameraPose(),
              [&camera](CameraPose* value) {
                *value = camera;
                return Result<void>();
              }));
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
