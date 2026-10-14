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
#include "render/comp_targets.h"
#include "render/effects.h"
#include "render/layer_painter.h"
#include "render/reel_text.h"

namespace snapper {
namespace {

QImage Blank(const Project& project, double scale, QColor color) {
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

// Cuts frame back to where before had something drawn, keeping soft
// edges soft (both premultiplied).
void KeepDrawnOnly(const QImage& before, QImage* frame) {
  assert(frame != nullptr);
  assert(before.size() == frame->size());
  QPainter painter(frame);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, before);
}

// Mixes the next clip in over a transition; mix runs 0 to 1.
QImage Combine(TransitionKind kind, double mix, const QImage& from,
               const QImage& to) {
  assert(from.size() == to.size());
  assert(mix >= 0.0 && mix <= 1.0);
  QImage out = from.copy();
  QPainter painter(&out);
  const double width = out.width();
  const double height = out.height();
  switch (kind) {
    case TransitionKind::kCut:
      painter.drawImage(0, 0, to);
      break;
    case TransitionKind::kCrossfade:
      painter.setOpacity(mix);
      painter.drawImage(0, 0, to);
      break;
    case TransitionKind::kFlash: {
      // Up to white over the first half, down into the next shot over
      // the second.
      const bool is_rising = mix < 0.5;
      if (!is_rising) {
        painter.drawImage(0, 0, to);
      }
      painter.setOpacity(is_rising ? mix * 2.0 : (1.0 - mix) * 2.0);
      painter.fillRect(out.rect(), Qt::white);
      break;
    }
    case TransitionKind::kSwipeLeft:
    case TransitionKind::kSwipeRight:
    case TransitionKind::kSwipeUp:
    case TransitionKind::kSwipeDown: {
      // The next shot pushes this one off the screen.
      const QPointF way =
          kind == TransitionKind::kSwipeLeft    ? QPointF(-width, 0)
          : kind == TransitionKind::kSwipeRight ? QPointF(width, 0)
          : kind == TransitionKind::kSwipeUp    ? QPointF(0, -height)
                                                : QPointF(0, height);
      out.fill(Qt::black);
      painter.drawImage(way * mix, from);
      painter.drawImage(way * (mix - 1.0), to);
      break;
    }
  }
  return out;
}

// Draws picture as large as fits in frame, centred, keeping its shape.
void DrawFitted(const QImage& picture, QImage* frame) {
  assert(frame != nullptr);
  assert(!picture.isNull());
  const QSizeF fitted =
      QSizeF(picture.size()).scaled(frame->size(), Qt::KeepAspectRatio);
  const QRectF target(QPointF((frame->width() - fitted.width()) / 2.0,
                              (frame->height() - fitted.height()) / 2.0),
                      fitted);
  QPainter painter(frame);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter.drawImage(target, picture);
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
                                 const QImage* under) {
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
  const CompNeeds needs = NeedsOf(shot, local, comp_show_);
  CompPictures pictures;
  if (needs.needs_drawn) {
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
      const double opacity =
          Sample(layer.transform, local, PiecePose()).opacity;
      // On a see-through shot an effect acts only where something is
      // drawn; a zoom punch moves what is drawn, so it is left alone.
      const bool is_masked = shot.is_transparent &&
                             effect->kind != EffectKind::kZoomPunch;
      const QImage before = is_masked ? frame.copy() : QImage();
      ApplyEffect(effect->kind,
                  Sample(effect->amount, local, 1.0) * opacity,
                  effect->color, scale, local.index(), &frame);
      if (is_masked) {
        KeepDrawnOnly(before, &frame);
      }
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
  const bool has_comp = !shot.comp.empty();
  if (has_comp) {
    const CompContext context{&project, &shot,     local,       scale,
                              view,     &links,    &pictures,   comp_show_};
    ApplyComp(context, &frame, &comp_report_);
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
  // What this layer covers is cut out of the targets below it, so an
  // effect on a doll never shows through what stands in front of it.
  for (auto& [key, picture] : pictures->targets) {
    QPainter painter(&picture);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawImage(0, 0, own);
  }
  // Each target joins the parts of every layer it names, bottom first.
  for (const CompTargetKey& key : needs.targets) {
    const CompTargetPart* part = PartOf(key, layer.id);
    const bool is_this = part != nullptr;
    if (!is_this) {
      continue;
    }
    QImage& picture = pictures->targets[key];
    if (picture.isNull()) {
      picture = Blank(project, scale, Qt::transparent);
    }
    QPainter painter(&picture);
    const bool is_whole = part->pieces.empty();
    if (is_whole) {
      painter.drawImage(0, 0, own);
      continue;
    }
    const std::set<QString> only(part->pieces.begin(), part->pieces.end());
    PaintLayer(project, layer, local, view, links, &cache_, &painter, &only);
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

void FrameRenderer::DrawClip(const Project& project, const Clip& clip,
                             Frame source, double scale, VideoFrames* videos,
                             QImage* out) {
  assert(out != nullptr);
  assert(scale > 0.0);
  const auto* video = std::get_if<VideoSource>(&clip.source);
  const bool is_video = video != nullptr;
  if (is_video) {
    const QImage picture = videos != nullptr
                               ? videos->Picture(video->path, source)
                               : QImage();
    const bool has_picture = !picture.isNull();
    if (has_picture) {
      DrawFitted(picture, out);
    }
    return;
  }
  const bool is_text = IsTextClip(clip);
  if (is_text) {
    QPainter painter(out);
    PaintReelText(project, clip, source.index() - clip.in.index(), scale,
                  &painter);
    return;
  }
  const Shot* shot =
      FindShot(project, std::get<ShotSource>(clip.source).shot);
  // A clip can outlive its shot; like the timeline, it shows nothing.
  const bool is_shot_gone = shot == nullptr;
  if (is_shot_gone) {
    return;
  }
  QPainter painter(out);
  painter.drawImage(0, 0, RenderShot(project, *shot, source, scale));
}

QImage FrameRenderer::RenderReel(const Project& project, Frame frame,
                                 double scale, VideoFrames* videos,
                                 QColor under) {
  assert(scale > 0.0);
  assert(frame.index() >= 0);
  QImage out = Blank(project, scale, under);
  for (const ReelPiece& piece : ReelAt(project, frame)) {
    // Text clips draw their own appear and leave; they never mix.
    const bool is_mixing = piece.next != nullptr && !IsTextClip(*piece.clip);
    if (!is_mixing) {
      DrawClip(project, *piece.clip, piece.source, scale, videos, &out);
      continue;
    }
    // Both sides are drawn over what lies below, then mixed.
    QImage from = out.copy();
    DrawClip(project, *piece.clip, piece.source, scale, videos, &from);
    QImage to = out.copy();
    DrawClip(project, *piece.next, piece.next_source, scale, videos, &to);
    out = Combine(piece.kind, piece.mix, from, to);
  }
  return out;
}

}  // namespace snapper
