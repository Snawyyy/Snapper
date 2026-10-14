#include "render/frame_renderer.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/jitter.h"
#include "anim/links.h"
#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "anim/sampler.h"
#include "render/comp.h"
#include "render/draft_paint.h"
#include "render/effects.h"
#include "render/layer_painter.h"

namespace snapper {

QImage FrameRenderer::Blank(const Project& project, double scale,
                            QColor color) {
  assert(scale > 0.0);
  assert(IsValidCanvas(project.canvas));
  const QSize size(
      std::max(1, static_cast<int>(std::lround(project.canvas.width * scale))),
      std::max(1,
               static_cast<int>(std::lround(project.canvas.height * scale))));
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(color);
  return image;
}

namespace {

// Cuts frame back to where before had something drawn, keeping soft
// edges soft (both premultiplied).
void KeepDrawnOnly(const QImage& before, QImage* frame) {
  assert(frame != nullptr);
  assert(before.size() == frame->size());
  QPainter painter(frame);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, before);
}

// ViewTransform, with or without the camera's shake.
QTransform ViewOf(const Project& project, const Shot& shot, Frame local,
                  double scale, bool is_shaken) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const CameraPose camera = Sample(shot.camera, local, CameraPose());
  const double shaking = is_shaken ? camera.shake : 0.0;
  const QPointF shake = QPointF(Jitter(local.index(), 0),
                                Jitter(local.index(), 1)) * shaking;
  const QPointF look = camera.center + shake;
  QTransform view = QTransform::fromTranslate(-look.x(), -look.y());
  view *= QTransform().rotate(-camera.rotation);
  view *= QTransform::fromScale(camera.zoom, camera.zoom);
  view *= QTransform::fromTranslate(project.canvas.width / 2.0,
                                    project.canvas.height / 2.0);
  return view * QTransform::fromScale(scale, scale);
}

// An effect layer over what is drawn so far.
void RunEffectLayer(const Shot& shot, const Layer& layer,
                    const EffectLayer& effect, Frame local, double scale,
                    QImage* frame) {
  assert(frame != nullptr);
  assert(scale > 0.0);
  const double opacity = Sample(layer.transform, local, PiecePose()).opacity;
  // On a see-through shot an effect acts only where something is
  // drawn; a zoom punch moves what is drawn, so it is left alone.
  const bool is_masked =
      shot.is_transparent && effect.kind != EffectKind::kZoomPunch;
  const QImage before = is_masked ? frame->copy() : QImage();
  ApplyEffect(effect.kind, Sample(effect.amount, local, 1.0) * opacity,
              effect.color, scale, local.index(), frame);
  if (is_masked) {
    KeepDrawnOnly(before, frame);
  }
}

}  // namespace

QTransform FrameRenderer::ViewTransform(const Project& project,
                                        const Shot& shot, Frame local,
                                        double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  return ViewOf(project, shot, local, scale, true);
}

QImage FrameRenderer::RenderShot(const Project& project, const Shot& shot,
                                 Frame local, double scale,
                                 const QImage* under, bool runs_screen,
                                 QImage* drawn) {
  assert(scale > 0.0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QImage frame = Blank(project, scale,
                       shot.is_transparent ? QColor(Qt::transparent)
                                           : shot.background);
  const bool has_under = under != nullptr && !under->isNull();
  if (has_under) {
    QPainter painter(&frame);
    painter.drawImage(0, 0, *under);
  }
  const QTransform view = ViewTransform(project, shot, local, scale);
  LinkSolver links(project, shot, cache_.poses());
  CompNeeds needs = NeedsOf(shot, local, comp_show_, runs_screen);
  const bool wants_drawn = drawn != nullptr;
  needs.needs_drawn = needs.needs_drawn || wants_drawn;
  CompPictures pictures;
  const bool needs_drawn = needs.needs_drawn;
  if (needs_drawn) {
    pictures.drawn = Blank(project, scale, Qt::transparent);
  }
  for (const Layer& layer : shot.layers) {
    const bool is_live = IsLayerLive(layer, local, shot.length);
    if (!is_live) {
      continue;
    }
    const auto* effect = std::get_if<EffectLayer>(&layer.content);
    const bool is_effect = effect != nullptr;
    if (is_effect) {
      RunEffectLayer(shot, layer, *effect, local, scale, &frame);
      continue;
    }
    const bool is_split = !needs.IsEmpty();
    if (is_split) {
      PaintForComp(project, layer, local, scale, view, &links, needs,
                   &pictures, &frame);
      continue;
    }
    QPainter painter(&frame);
    PaintLayer(project, layer, local, view, &links, &cache_, &painter);
  }
  if (wants_drawn) {
    *drawn = pictures.drawn;
  }
  const bool has_comp = !shot.comp.empty();
  if (has_comp) {
    CompContext context{&project, &shot,     local,       scale,
                        view,     &links,    &pictures,   comp_show_};
    context.skips_screen = !runs_screen;
    ApplyComp(context, &frame, &comp_report_);
  }
  if (is_draft_shown_) {
    PaintDraft(shot.draft, local, &cache_, &frame);
  }
  return frame;
}

void FrameRenderer::PaintForComp(const Project& project, const Layer& layer,
                                 Frame local,
                                 double scale, const QTransform& view,
                                 LinkSolver* links, const CompNeeds& needs,
                                 CompPictures* pictures, QImage* frame) {
  assert(pictures != nullptr && frame != nullptr);
  assert(!needs.IsEmpty());
  QImage own = Blank(project, scale, Qt::transparent);
  {
    QPainter painter(&own);
    PaintLayer(project, layer, local, view, links, &cache_, &painter);
  }
  // What this layer covers is cut out of the targets below it and joins
  // their covers, so an effect on a doll stays behind what stands in
  // front of it.
  for (auto& [key, picture] : pictures->targets) {
    QPainter painter(&picture);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawImage(0, 0, own);
    QImage& cover = pictures->covers[key];
    const bool is_first_cover = cover.isNull();
    if (is_first_cover) {
      cover = Blank(project, scale, Qt::transparent);
    }
    QPainter covering(&cover);
    covering.drawImage(0, 0, own);
  }
  for (const CompTargetKey& key : needs.targets) {
    const bool is_this = key.layer == layer.id;
    if (!is_this) {
      continue;
    }
    const bool is_whole = key.pieces.empty();
    if (is_whole) {
      pictures->targets[key] = own;
      continue;
    }
    // The doll's own pieces above the picked ones cover them too.
    const std::set<QString> only(key.pieces.begin(), key.pieces.end());
    for (const PieceSplit split : {PieceSplit::kShown, PieceSplit::kCover}) {
      QImage picture = Blank(project, scale, Qt::transparent);
      QPainter painter(&picture);
      PaintLayer(project, layer, local, view, links, &cache_, &painter, &only,
                 split);
      painter.end();
      const bool is_shown = split == PieceSplit::kShown;
      (is_shown ? pictures->targets : pictures->covers)[key] = picture;
    }
  }
  QPainter painter(frame);
  painter.drawImage(0, 0, own);
  const bool has_drawn = !pictures->drawn.isNull();
  if (has_drawn) {
    QPainter drawn(&pictures->drawn);
    drawn.drawImage(0, 0, own);
  }
}

QImage FrameRenderer::RenderLayers(
    const Project& project, const Shot& shot, Frame local, double scale,
    const std::map<LayerId, std::set<QString>>& only) {
  assert(scale > 0.0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QImage frame = Blank(project, scale, Qt::transparent);
  const QTransform view = ViewTransform(project, shot, local, scale);
  LinkSolver links(project, shot, cache_.poses());
  QPainter painter(&frame);
  for (const Layer& layer : shot.layers) {
    const auto chosen = only.find(layer.id);
    const bool is_chosen = chosen != only.end() &&
                           IsLayerLive(layer, local, shot.length) &&
                           !std::holds_alternative<EffectLayer>(layer.content);
    if (is_chosen) {
      const bool is_whole = chosen->second.empty();
      PaintLayer(project, layer, local, view, &links, &cache_, &painter,
                 is_whole ? nullptr : &chosen->second);
    }
  }
  return frame;
}

QImage FrameRenderer::RenderArea(const Project& project, const Shot& shot,
                                 Frame local, double scale,
                                 const QRect& area) {
  assert(scale > 0.0);
  assert(area.width() > 0 && area.height() > 0);
  QImage out(area.size(), QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  const QTransform view = ViewOf(project, shot, local, scale, false) *
                          QTransform::fromTranslate(-area.left(),
                                                    -area.top());
  LinkSolver links(project, shot, cache_.poses());
  QPainter painter(&out);
  for (const Layer& layer : shot.layers) {
    const bool is_drawn = IsLayerLive(layer, local, shot.length) &&
                          !std::holds_alternative<EffectLayer>(layer.content);
    if (is_drawn) {
      PaintLayer(project, layer, local, view, &links, &cache_, &painter);
    }
  }
  return out;
}

QImage FrameRenderer::RenderFrame(const Project& project, Frame master,
                                  double scale, const QImage* under) {
  assert(scale > 0.0);
  assert(master.index() >= 0);
  const ShotMoment moment = Locate(project, master);
  const bool is_past_end = moment.shot < 0;
  if (is_past_end) {
    return Blank(project, scale, Qt::black);
  }
  const Shot& shot = *project.shots[static_cast<size_t>(moment.shot)];
  return RenderShot(project, shot, moment.local, scale, under);
}

}  // namespace snapper
