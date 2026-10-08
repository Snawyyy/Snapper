#include "edit/shot_manager.h"

#include <algorithm>
#include <cassert>
#include <map>

#include "edit/history_manager.h"
#include "edit/project_edits.h"

namespace snapper {

QString TransitionName(TransitionKind kind) {
  assert(static_cast<int>(kind) < kTransitionKindCount);
  assert(kTransitionKindCount == 7);
  switch (kind) {
    case TransitionKind::kCut:
      return Tr("Cut");
    case TransitionKind::kSwipeLeft:
      return Tr("Swipe left");
    case TransitionKind::kSwipeRight:
      return Tr("Swipe right");
    case TransitionKind::kSwipeUp:
      return Tr("Swipe up");
    case TransitionKind::kSwipeDown:
      return Tr("Swipe down");
    case TransitionKind::kFlash:
      return Tr("Flash");
    case TransitionKind::kCrossfade:
      return Tr("Crossfade");
  }
  return QString();
}

ShotManager::ShotManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<ShotId> ShotManager::Add(int index) {
  assert(history_ != nullptr);
  assert(index >= -1);
  Project next = history_->current();
  const int count = static_cast<int>(next.shots.size());
  const bool is_full = count >= kMaxShots;
  if (is_full) {
    return std::unexpected(
        Error{Tr("A project holds at most %1 shots.").arg(kMaxShots)});
  }
  Shot shot;
  shot.id = TakeShotId(&next);
  shot.name = Tr("Shot %1").arg(shot.id.value());
  const int at = index < 0 || index > count ? count : index;
  next.shots.insert(next.shots.begin() + at,
                    std::make_shared<const Shot>(shot));
  auto applied = history_->Apply(Tr("Add shot"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return shot.id;
}

Result<ShotId> ShotManager::Duplicate(ShotId shot) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Project next = history_->current();
  const int index = ShotIndex(next, shot);
  const bool can_copy =
      index >= 0 && next.shots.size() < static_cast<size_t>(kMaxShots);
  if (!can_copy) {
    return std::unexpected(Error{Tr("That shot can't be copied.")});
  }
  Shot copy = *next.shots[static_cast<size_t>(index)];
  copy.id = TakeShotId(&next);
  copy.name = Tr("%1 copy").arg(copy.name);
  std::map<LayerId, LayerId> renamed;
  for (Layer& layer : copy.layers) {
    const LayerId fresh = TakeLayerId(&next);
    renamed[layer.id] = fresh;
    layer.id = fresh;
  }
  // Links in the copy join the copy's own layers. A link to a layer the
  // shot no longer has stays dead in the copy, so DropDeadLinks clears it.
  for (Link& link : copy.links) {
    const auto follower = renamed.find(link.follower.layer);
    const auto leader = renamed.find(link.leader.layer);
    link.follower.layer =
        follower != renamed.end() ? follower->second : LayerId();
    link.leader.layer = leader != renamed.end() ? leader->second : LayerId();
  }
  DropDeadLinks(&copy);
  next.shots.insert(next.shots.begin() + index + 1,
                    std::make_shared<const Shot>(copy));
  auto applied = history_->Apply(Tr("Duplicate shot"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return copy.id;
}

Result<void> ShotManager::Remove(ShotId shot) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Project next = history_->current();
  const int index = ShotIndex(next, shot);
  const bool is_present = index >= 0;
  if (!is_present) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  next.shots.erase(next.shots.begin() + index);
  return history_->Apply(Tr("Remove shot"), std::move(next));
}

Result<void> ShotManager::Move(ShotId shot, int index) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  Project next = history_->current();
  const int from = ShotIndex(next, shot);
  const int count = static_cast<int>(next.shots.size());
  const bool is_valid = from >= 0 && index >= 0 && index < count;
  if (!is_valid) {
    return std::unexpected(Error{Tr("A shot can't go there.")});
  }
  auto moved = next.shots[static_cast<size_t>(from)];
  next.shots.erase(next.shots.begin() + from);
  next.shots.insert(next.shots.begin() + index, std::move(moved));
  return history_->Apply(Tr("Move shot"), std::move(next));
}

Result<void> ShotManager::SetLength(ShotId shot, Frame length) {
  assert(history_ != nullptr);
  assert(length.index() >= 0);
  const bool is_valid = length.index() >= 1;
  if (!is_valid) {
    return std::unexpected(Error{Tr("A shot is at least one frame long.")});
  }
  return history_->Apply(
      Tr("Change shot length"),
      WithShot(history_->current(), shot, [length](Shot* edited) {
        edited->length = length;
        return Result<void>();
      }));
}

Result<void> ShotManager::Rename(ShotId shot, const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  const bool is_blank = name.trimmed().isEmpty();
  if (is_blank) {
    return std::unexpected(Error{Tr("A shot needs a name.")});
  }
  return history_->Apply(
      Tr("Rename shot"),
      WithShot(history_->current(), shot, [&name](Shot* edited) {
        edited->name = name.trimmed();
        return Result<void>();
      }));
}

Result<void> ShotManager::SetBackground(ShotId shot, QColor color) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool is_valid = color.isValid();
  if (!is_valid) {
    return std::unexpected(Error{Tr("That isn't a colour.")});
  }
  return history_->Apply(
      Tr("Change background"),
      WithShot(history_->current(), shot, [color](Shot* edited) {
        edited->background = color;
        return Result<void>();
      }));
}

Result<void> ShotManager::SetTransition(ShotId shot, Transition transition) {
  assert(history_ != nullptr);
  assert(static_cast<int>(transition.kind) < kTransitionKindCount);
  return history_->Apply(
      Tr("Change transition"),
      WithShot(history_->current(), shot, [transition](Shot* edited) {
        edited->transition = transition;
        return Result<void>();
      }));
}

}  // namespace snapper
