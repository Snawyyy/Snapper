// PoseManager's whole-doll poses: IK drags, copy and paste.

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_pose.h"
#include "anim/ik.h"
#include "anim/mirror.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/pose_manager.h"

namespace snapper {
namespace {

// The project with every piece in poses keyed at frame on a layer.
Result<Project> KeyPoses(Project project, ShotId shot, LayerId layer,
                         Frame frame, const PoseMap& poses) {
  assert(frame.index() >= 0);
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  for (const auto& [piece, pose] : poses) {
    const TrackRef track{shot, TrackKind::kPiece, layer, piece};
    auto next = KeyedAt(project, track, frame, PiecePose(),
                        [&pose](PiecePose* value) {
                          *value = pose;
                          return Result<void>();
                        });
    if (!next) {
      return next;
    }
    project = std::move(*next);
  }
  return project;
}

}  // namespace

Result<void> PoseManager::DragIk(ShotId shot, LayerId layer,
                                 const QString& chain, Frame frame,
                                 QPointF target) {
  assert(history_ != nullptr);
  assert(!chain.isEmpty());
  const Project& project = history_->current();
  const Doll* doll = DollOfLayer(project, shot, layer);
  const IkChain* found = doll != nullptr ? FindChain(doll->rig, chain)
                                         : nullptr;
  const bool is_finite = std::isfinite(target.x()) && std::isfinite(target.y());
  const bool is_ready = found != nullptr && is_finite;
  if (!is_ready) {
    return std::unexpected(Error{Tr("That doll has no IK chain %1.")
                                     .arg(chain)});
  }
  PoseMap poses = SamplePoses(*PosedLayerOf(project, shot, layer), frame);
  const auto solved = SolveIk(*doll, poses, *found, target);
  if (!solved) {
    return std::unexpected(Error{
        Tr("IK chain %1 has a bone with no length; move its tip in the rig.")
            .arg(chain)});
  }
  PoseMap bent;
  bent[found->upper] = poses[found->upper];
  bent[found->lower] = poses[found->lower];
  bent[found->upper].rotation = solved->upper_rotation;
  bent[found->lower].rotation = solved->lower_rotation;
  return history_->Apply(Tr("Bend %1").arg(chain),
                         KeyPoses(project, shot, layer, frame, bent));
}

Result<void> PoseManager::CopyPose(ShotId shot, LayerId layer, Frame frame,
                                   const std::set<QString>& pieces) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  const Doll* doll = DollOfLayer(project, shot, layer);
  const bool is_doll = doll != nullptr;
  if (!is_doll) {
    return std::unexpected(Error{Tr("Pick a doll layer first.")});
  }
  const PoseMap poses =
      SamplePoses(*PosedLayerOf(project, shot, layer), frame);
  std::map<QString, PiecePose> copied;
  for (const RigPiece& piece : doll->rig.pieces) {
    const bool is_wanted = pieces.empty() || pieces.contains(piece.name);
    if (is_wanted) {
      const auto found = poses.find(piece.name);
      copied[piece.name] =
          found == poses.end() ? PiecePose() : found->second;
    }
  }
  clipboard_ = std::move(copied);
  return {};
}

Result<void> PoseManager::PastePose(ShotId shot, LayerId layer, Frame frame,
                                    bool is_mirrored) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoPaste();
  const bool can_paste = why_not.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why_not});
  }
  const Project& project = history_->current();
  const Doll* doll = DollOfLayer(project, shot, layer);
  const bool is_doll = doll != nullptr;
  if (!is_doll) {
    return std::unexpected(Error{Tr("Pick a doll layer first.")});
  }
  PoseMap pasted;
  for (const auto& [name, pose] : clipboard_) {
    const QString partner = is_mirrored ? MirrorName(name) : name;
    const RigPiece* own = FindRig(doll->rig, partner);
    const RigPiece* target = own != nullptr ? own : FindRig(doll->rig, name);
    const bool fits = target != nullptr;
    if (fits) {
      pasted[target->name] =
          is_mirrored ? MirrorPose(pose, target->warp) : pose;
    }
  }
  return history_->Apply(is_mirrored ? Tr("Paste mirrored pose")
                                     : Tr("Paste pose"),
                         KeyPoses(project, shot, layer, frame, pasted));
}

QString PoseManager::WhyNoPaste() const {
  assert(history_ != nullptr);
  assert(clipboard_.size() <= static_cast<size_t>(kMaxPieceTracks));
  const bool is_empty = clipboard_.empty();
  return is_empty ? Tr("Copy a pose first.") : QString();
}

}  // namespace snapper
