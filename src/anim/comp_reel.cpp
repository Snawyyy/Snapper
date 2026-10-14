#include "anim/comp_reel.h"

#include <algorithm>
#include <cassert>
#include <utility>
#include <variant>

namespace snapper {

const Clip* FirstClipOf(const Project& project, ShotId shot) {
  assert(shot.value() >= 0);
  assert(project.reel.tracks.size() < 10000);
  const Clip* first = nullptr;
  for (const ReelTrack& track : project.reel.tracks) {
    const bool is_picture = !track.is_sound && !track.is_subtitle;
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      const bool is_earlier =
          is_picture && source != nullptr && source->shot == shot &&
          (first == nullptr || clip.start.index() < first->start.index());
      first = is_earlier ? &clip : first;
    }
  }
  return first;
}

ReelSpan EffectOnReel(const Clip& clip, const CompEffect& effect) {
  assert(clip.start.index() >= 0);
  assert(effect.length == kWholeShot || effect.length >= 0);
  const bool is_whole = effect.length == kWholeShot;
  if (is_whole) {
    return ReelSpan{clip.start.index(), clip.end().index()};
  }
  const int offset = clip.start.index() - clip.in.index();
  return ReelSpan{effect.start + offset,
                  effect.start + effect.length + offset};
}

Frame EffectFrameAt(const Project& project, ShotId shot, Frame reel) {
  assert(reel.index() >= 0);
  assert(shot.value() >= 0);
  const Clip* clip = FirstClipOf(project, shot);
  const Shot* found = FindShot(project, shot);
  const bool is_on_reel = clip != nullptr && found != nullptr;
  if (!is_on_reel) {
    return Frame(0);
  }
  const int at = reel.index() - clip->start.index() + clip->in.index();
  return Frame(std::clamp(at, 0, std::max(0, found->length.index() - 1)));
}

std::vector<ReelEffect> ScreenEffectsAt(const Project& project,
                                        Frame frame) {
  assert(frame.index() >= 0);
  assert(project.shots.size() < 100000);
  std::vector<std::pair<const Clip*, const Shot*>> shots;
  for (const auto& shot : project.shots) {
    const Clip* clip = shot->comp.empty() ? nullptr
                                          : FirstClipOf(project, shot->id);
    const bool is_on_reel = clip != nullptr;
    if (is_on_reel) {
      shots.emplace_back(clip, shot.get());
    }
  }
  std::ranges::stable_sort(shots, [](const auto& a, const auto& b) {
    return a.first->start.index() < b.first->start.index();
  });
  std::vector<ReelEffect> running;
  const int reel = frame.index();
  for (const auto& [clip, shot] : shots) {
    const int offset = clip->start.index() - clip->in.index();
    for (const CompEffect& effect : shot->comp) {
      const ReelSpan span = EffectOnReel(*clip, effect);
      const bool is_running = !effect.target.layer.IsValid() &&
                              reel >= span.first && reel < span.end;
      if (is_running) {
        running.push_back(ReelEffect{shot, &effect, reel - offset});
      }
    }
  }
  return running;
}

}  // namespace snapper
