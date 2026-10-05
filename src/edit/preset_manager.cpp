#include "edit/preset_manager.h"

#include <algorithm>
#include <cassert>
#include <type_traits>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"

namespace snapper {
namespace {

// The track's pose at frame as it is now.
PiecePose PoseNow(const Project& project, const TrackRef& track,
                  Frame frame) {
  assert(frame.index() >= 0);
  assert(track.kind == TrackKind::kLayer || track.kind == TrackKind::kPiece);
  PiecePose pose;
  const Shot* shot = FindShot(project, track.shot);
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    ReadTrack(*shot, track, [&](const auto& channel) {
      using Held = typename std::remove_cvref_t<decltype(channel)>::value_type;
      constexpr bool is_pose = std::is_same_v<Held, PiecePose>;
      if constexpr (is_pose) {
        pose = Sample(channel, frame, PiecePose());
      }
    });
  }
  return pose;
}

// Keys pose at frame on track, replacing the whole pose.
Result<Project> KeyPose(const Project& project, const TrackRef& track,
                        Frame frame, const PiecePose& pose) {
  assert(frame.index() >= 0);
  assert(track.shot.value() >= 0);
  return KeyedAt(project, track, frame, PiecePose(),
                 [&pose](PiecePose* value) {
                   *value = pose;
                   return Result<void>();
                 });
}

// The project with motion laid onto one track.
Result<Project> WithMotion(Project project, const TrackRef& track,
                           Frame start, const PresetSettings& settings) {
  assert(start.index() >= 0);
  assert(settings.length > 0);
  const Frame end(start.index() + settings.length);
  const PiecePose after = PoseNow(project, track, end);
  // Bases are read before writing, so the loop rides on the old motion.
  std::vector<Key<PiecePose>> keys = PresetKeys(settings);
  for (Key<PiecePose>& key : keys) {
    key.frame = Frame(start.index() + key.frame.index());
    key.value = AddPose(PoseNow(project, track, key.frame), key.value);
  }
  for (const Key<PiecePose>& key : keys) {
    auto next = KeyedAt(project, track, key.frame, PiecePose(),
                        [&key](PiecePose* value) {
                          *value = key.value;
                          return Result<void>();
                        });
    if (!next) {
      return next;
    }
    project = std::move(*next);
  }
  return KeyPose(project, track, end, after);
}

}  // namespace

PresetManager::PresetManager(HistoryManager* history, QString poses_path)
    : history_(history), poses_path_(std::move(poses_path)) {
  assert(history_ != nullptr);
  assert(!poses_path_.isEmpty());
  auto read = ReadPoses(poses_path_);
  if (read) {
    poses_ = std::move(*read);
  } else {
    // Kept, never overwritten: saving refuses until the file is fixed.
    load_error_ = read.error().message;
  }
}

Result<void> PresetManager::ApplyMotion(const std::vector<TrackRef>& tracks,
                                        Frame start,
                                        const PresetSettings& settings) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  const bool is_valid = !tracks.empty() && settings.hold >= 1 &&
                        settings.length >= 1 &&
                        start.index() + settings.length <= kMaxFrame;
  if (!is_valid) {
    return std::unexpected(
        Error{Tr("Pick a piece or layer and a loop at least one frame long.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    const bool is_posable =
        track.kind == TrackKind::kLayer || track.kind == TrackKind::kPiece;
    if (!is_posable) {
      return std::unexpected(
          Error{Tr("Motion goes on pieces and layers only.")});
    }
    auto moved = WithMotion(std::move(next), track, start, settings);
    if (!moved) {
      return std::unexpected(moved.error());
    }
    next = std::move(*moved);
  }
  return history_->Apply(Tr("Add %1").arg(PresetName(settings.preset)),
                         std::move(next));
}

}  // namespace snapper
