// FrameRenderer: the screen effects of every shot, run over the finished
// reel picture wherever their clips reach on the Comp timeline, past
// their shot's cut too.

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <memory>

#include "anim/comp_reel.h"
#include "anim/links.h"
#include "render/comp.h"
#include "render/comp_targets.h"
#include "render/frame_renderer.h"

namespace snapper {

bool FrameRenderer::NeedsReelShape(const Project& project, Frame frame) {
  assert(frame.index() >= 0);
  assert(project.shots.size() < 100000);
  const std::vector<ReelEffect> running = ScreenEffectsAt(project, frame);
  return std::ranges::any_of(running, [](const ReelEffect& one) {
    return NeedsCompShape(*one.effect);
  });
}

void FrameRenderer::ApplyReelComp(const Project& project, Frame frame,
                                  double scale, const QImage& drawn,
                                  QImage* out) {
  assert(out != nullptr && !out->isNull());
  assert(scale > 0.0);
  const std::vector<ReelEffect> running = ScreenEffectsAt(project, frame);
  CompPictures pictures;
  pictures.drawn = drawn;
  const Shot* solver_shot = nullptr;
  std::unique_ptr<LinkSolver> links;
  for (const ReelEffect& one : running) {
    const Shot& shot = *one.shot;
    // One solver per shot: the effects come shot by shot.
    const bool is_new_shot = &shot != solver_shot;
    if (is_new_shot) {
      links = std::make_unique<LinkSolver>(project, shot);
      solver_shot = &shot;
    }
    const Frame local(
        std::clamp(one.at, 0, std::max(0, shot.length.index() - 1)));
    CompContext context{&project,
                        &shot,
                        local,
                        scale,
                        ViewTransform(project, shot, local, scale),
                        links.get(),
                        &pictures,
                        comp_show_};
    context.only = one.effect;
    ApplyComp(context, out, &comp_report_);
  }
}

}  // namespace snapper
