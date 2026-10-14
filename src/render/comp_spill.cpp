// FrameRenderer: screen effects that run on past their shot's clip on
// the reel, drawn over whatever the reel shows there.

#include <algorithm>
#include <cassert>
#include <variant>

#include "anim/links.h"
#include "render/comp.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

// True when shot has a screen effect timed by hand, the only kind that
// can run past its clip.
bool CanSpill(const Shot& shot) {
  assert(shot.comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  assert(shot.length.index() >= 0);
  return std::ranges::any_of(shot.comp, [](const CompEffect& effect) {
    return effect.length != kWholeShot && !effect.target.layer.IsValid();
  });
}

}  // namespace

void FrameRenderer::ApplySpills(const Project& project, Frame frame,
                                double scale, QImage* out) {
  assert(out != nullptr);
  assert(scale > 0.0);
  for (const ReelTrack& track : project.reel.tracks) {
    const bool is_picture = !track.is_sound && !track.is_subtitle;
    if (!is_picture) {
      continue;
    }
    for (const Clip& clip : track.clips) {
      SpillClip(project, clip, frame, scale, out);
    }
  }
}

void FrameRenderer::SpillClip(const Project& project, const Clip& clip,
                              Frame frame, double scale, QImage* out) {
  assert(out != nullptr);
  assert(scale > 0.0);
  const auto* source = std::get_if<ShotSource>(&clip.source);
  const Shot* shot =
      source != nullptr ? FindShot(project, source->shot) : nullptr;
  const bool can_spill = shot != nullptr && CanSpill(*shot);
  const int reel = frame.index();
  // Inside its own clip the shot drew its effects itself.
  const bool is_own = reel >= clip.start.index() && reel < clip.end().index();
  const bool is_skipped = !can_spill || is_own;
  if (is_skipped) {
    return;
  }
  const int at = reel - clip.start.index() + clip.in.index();
  const Frame local(std::clamp(at, 0, std::max(0, shot->length.index() - 1)));
  LinkSolver links(project, *shot, cache_.poses());
  CompPictures pictures;
  // Past the shot there are no layers: the whole picture is the shape.
  pictures.drawn = out->copy();
  CompContext context{&project,
                      shot,
                      local,
                      scale,
                      ViewTransform(project, *shot, local, scale),
                      &links,
                      &pictures,
                      comp_show_};
  context.is_spill = true;
  context.at = at;
  ApplyComp(context, out, &comp_report_);
}

}  // namespace snapper
