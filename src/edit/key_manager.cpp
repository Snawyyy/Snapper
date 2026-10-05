#include "edit/key_manager.h"

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/key_edits.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// Picked keys grouped by channel.
std::map<TrackRef, std::set<Frame>> ByTrack(const std::set<KeyRef>& keys) {
  assert(keys.size() < 10000000);
  std::map<TrackRef, std::set<Frame>> grouped;
  for (const KeyRef& key : keys) {
    grouped[key.track].insert(key.frame);
  }
  assert(grouped.size() <= keys.size());
  return grouped;
}

// Applies change(channel, frames) to each picked channel in turn.
template <typename Change>
Result<Project> ForEachTrack(Project project, const std::set<KeyRef>& keys,
                             Change change) {
  assert(keys.size() < 10000000);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  for (const auto& [track, frames] : ByTrack(keys)) {
    auto next = WithTrack(project, track, [&](auto* channel) {
      return change(channel, frames);
    });
    if (!next) {
      return next;
    }
    project = std::move(*next);
  }
  return project;
}

}  // namespace

KeyManager::KeyManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(clipboard_.empty());
}

Result<std::set<KeyRef>> KeyManager::Shift(const std::set<KeyRef>& keys,
                                           int delta) {
  assert(history_ != nullptr);
  assert(keys.size() < 10000000);
  std::set<KeyRef> moved;
  for (const KeyRef& key : keys) {
    const int frame = key.frame.index() + delta;
    const bool is_on_timeline = frame >= 0 && frame <= kMaxFrame;
    if (!is_on_timeline) {
      return std::unexpected(
          Error{Tr("Keys can't move before the start of the shot.")});
    }
    moved.insert({key.track, Frame(frame)});
  }
  auto next = ForEachTrack(
      history_->current(), keys,
      [delta](auto* channel, const std::set<Frame>& frames) {
        auto picked = channel->keys;
        std::erase_if(picked, [&](const auto& k) {
          return !frames.contains(k.frame);
        });
        std::erase_if(channel->keys, [&](const auto& k) {
          return frames.contains(k.frame);
        });
        for (auto& key : picked) {
          key.frame = Frame(key.frame.index() + delta);
          const bool is_set = SetKey(channel, key);
          if (!is_set) {
            return Result<void>(
                std::unexpected(Error{Tr("That track is full of keys.")}));
          }
        }
        return Result<void>();
      });
  auto applied = history_->Apply(Tr("Move keys"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return moved;
}

Result<void> KeyManager::Remove(const std::set<KeyRef>& keys) {
  assert(history_ != nullptr);
  assert(keys.size() < 10000000);
  const bool is_empty = keys.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick some keys first.")});
  }
  return history_->Apply(
      Tr("Delete keys"),
      ForEachTrack(history_->current(), keys,
                   [](auto* channel, const std::set<Frame>& frames) {
                     std::erase_if(channel->keys, [&](const auto& k) {
                       return frames.contains(k.frame);
                     });
                     return Result<void>();
                   }));
}

Result<void> KeyManager::SetEase(const std::set<KeyRef>& keys, Ease ease) {
  assert(history_ != nullptr);
  assert(static_cast<int>(ease) < kEaseCount);
  return history_->Apply(
      Tr("Change ease"),
      ForEachTrack(history_->current(), keys,
                   [ease](auto* channel, const std::set<Frame>& frames) {
                     for (auto& key : channel->keys) {
                       const bool is_picked = frames.contains(key.frame);
                       key.ease = is_picked ? ease : key.ease;
                     }
                     return Result<void>();
                   }));
}

Result<void> KeyManager::Retime(const std::vector<TrackRef>& tracks,
                                Frame frame, int delta) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_valid = delta != 0 && std::abs(delta) <= kMaxFrame;
  if (!is_valid) {
    return std::unexpected(Error{Tr("Add or take out at least one frame.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    auto slid = WithTrack(next, track, [&](auto* channel) {
      const bool fits = SlideFrom(channel, frame, delta);
      return fits ? Result<void>()
                  : Result<void>(std::unexpected(
                        Error{Tr("That would push keys off the timeline.")}));
    });
    if (!slid) {
      return std::unexpected(slid.error());
    }
    next = std::move(*slid);
  }
  return history_->Apply(delta > 0 ? Tr("Insert frames") : Tr("Remove frames"),
                         std::move(next));
}

}  // namespace snapper
