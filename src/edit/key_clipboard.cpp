// KeyManager's copying: repeat, copy and paste.

#include <algorithm>
#include <cassert>
#include <type_traits>

#include "edit/history_manager.h"
#include "edit/key_edits.h"
#include "edit/key_manager.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

constexpr int kMaxRepeats = 1000;

// Where copied keys from track go when pasted into shot (and onto a
// layer, if one is given).
TrackRef PasteTarget(TrackRef track, ShotId shot, LayerId onto) {
  assert(shot.value() >= 0);
  assert(onto.value() >= 0);
  track.shot = shot;
  const bool is_moved = onto.IsValid() && track.kind != TrackKind::kCamera;
  if (is_moved) {
    track.layer = onto;
  }
  return track;
}

// Copies of the keys in [start, end) laid down times more after end.
template <typename T>
bool RepeatKeys(Channel<T>* channel, Frame start, Frame end, int times) {
  assert(channel != nullptr);
  assert(start < end && times >= 1);
  const int length = end.index() - start.index();
  std::vector<Key<T>> loop;
  for (const Key<T>& key : channel->keys) {
    const bool is_inside = !(key.frame < start) && key.frame < end;
    if (is_inside) {
      loop.push_back(key);
    }
  }
  const bool has_room = SlideFrom(channel, end, length * times);
  if (!has_room) {
    return false;
  }
  for (int round = 1; round <= times; ++round) {
    for (Key<T> key : loop) {
      key.frame = Frame(key.frame.index() + length * round);
      const bool is_set = SetKey(channel, key);
      if (!is_set) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

Result<void> KeyManager::Repeat(const std::vector<TrackRef>& tracks,
                                Frame start, Frame end, int times) {
  assert(history_ != nullptr);
  assert(tracks.size() < 100000);
  const long long added =
      static_cast<long long>(end.index() - start.index()) * times;
  const bool is_valid = start < end && times >= 1 && times <= kMaxRepeats &&
                        added <= kMaxFrame;
  if (!is_valid) {
    return std::unexpected(Error{Tr(
        "Pick a range of at least one frame and repeat it 1 to 1000 times.")});
  }
  Project next = history_->current();
  for (const TrackRef& track : tracks) {
    auto repeated = WithTrack(next, track, [&](auto* channel) {
      const bool fits = RepeatKeys(channel, start, end, times);
      return fits ? Result<void>()
                  : Result<void>(std::unexpected(
                        Error{Tr("That would push keys off the timeline.")}));
    });
    if (!repeated) {
      return std::unexpected(repeated.error());
    }
    next = std::move(*repeated);
  }
  return history_->Apply(Tr("Repeat keys"), std::move(next));
}

Result<void> KeyManager::Copy(const std::set<KeyRef>& keys) {
  assert(history_ != nullptr);
  assert(keys.size() < 10000000);
  const bool is_empty = keys.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick some keys first.")});
  }
  const int first = std::min_element(keys.begin(), keys.end(),
                                     [](const KeyRef& a, const KeyRef& b) {
                                       return a.frame < b.frame;
                                     })
                        ->frame.index();
  std::map<TrackRef, CopiedChannel> copied;
  for (const KeyRef& key : keys) {
    const Shot* shot = FindShot(history_->current(), key.track.shot);
    const bool has_shot = shot != nullptr;
    if (!has_shot) {
      continue;
    }
    ReadTrack(*shot, key.track, [&](const auto& channel) {
      using Held = std::remove_cvref_t<decltype(channel)>;
      const int at = KeyIndexAt(channel, key.frame);
      const bool is_key = at >= 0;
      if (is_key) {
        auto& out = copied.try_emplace(key.track, Held()).first->second;
        auto picked = channel.keys[static_cast<size_t>(at)];
        picked.frame = Frame(picked.frame.index() - first);
        [[maybe_unused]] const bool is_set =
            SetKey(&std::get<Held>(out), picked);
        assert(is_set);
      }
    });
  }
  clipboard_ = std::move(copied);
  return {};
}

Result<void> KeyManager::Paste(ShotId shot, Frame frame, LayerId onto) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoPaste();
  const bool can_paste = why_not.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  for (const auto& [track, copied] : clipboard_) {
    auto pasted = WithTrack(
        next, PasteTarget(track, shot, onto), [&](auto* channel) {
          using Held = std::remove_pointer_t<decltype(channel)>;
          const auto* keys = std::get_if<Held>(&copied);
          const bool is_same_kind = keys != nullptr;
          if (!is_same_kind) {
            return Result<void>(std::unexpected(
                Error{Tr("Those keys don't fit that layer.")}));
          }
          for (auto key : keys->keys) {
            key.frame = Frame(key.frame.index() + frame.index());
            const bool is_set = SetKey(channel, key);
            if (!is_set) {
              return Result<void>(std::unexpected(
                  Error{Tr("That track is full of keys.")}));
            }
          }
          return Result<void>();
        });
    if (!pasted) {
      return std::unexpected(pasted.error());
    }
    next = std::move(*pasted);
  }
  return history_->Apply(Tr("Paste keys"), std::move(next));
}

QString KeyManager::WhyNoPaste() const {
  assert(history_ != nullptr);
  assert(clipboard_.size() < 100000);
  const bool is_empty = clipboard_.empty();
  return is_empty ? Tr("Copy some keys first.") : QString();
}

}  // namespace snapper
