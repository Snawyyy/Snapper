#include "render/frame_renderer.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/jitter.h"
#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "anim/sampler.h"
#include "render/effects.h"
#include "render/layer_painter.h"

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

// Mixes the next shot in over a transition; mix runs 0 to 1.
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
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.drawImage(target, picture);
}

}  // namespace

QTransform FrameRenderer::ViewTransform(const Project& project,
                                        const Shot& shot, Frame local,
                                        double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const CameraPose camera = Sample(shot.camera, local, CameraPose());
  const QPointF shake = QPointF(Jitter(local.index(), 0),
                                Jitter(local.index(), 1)) * camera.shake;
  const QPointF look = camera.center + shake;
  QTransform view = QTransform::fromTranslate(-look.x(), -look.y());
  view *= QTransform().rotate(-camera.rotation);
  view *= QTransform::fromScale(camera.zoom, camera.zoom);
  view *= QTransform::fromTranslate(project.canvas.width / 2.0,
                                    project.canvas.height / 2.0);
  return view * QTransform::fromScale(scale, scale);
}

QImage FrameRenderer::RenderShot(const Project& project, const Shot& shot,
                                 Frame local, double scale) {
  assert(scale > 0.0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QImage frame = Blank(project, scale, shot.background);
  const QTransform view = ViewTransform(project, shot, local, scale);
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
      ApplyEffect(effect->kind,
                  Sample(effect->amount, local, 1.0) * opacity,
                  effect->color, scale, local.index(), &frame);
      continue;
    }
    QPainter painter(&frame);
    PaintLayer(project, layer, local, view, &cache_, &painter);
  }
  return frame;
}

QImage FrameRenderer::RenderFrame(const Project& project, Frame master,
                                  double scale) {
  assert(scale > 0.0);
  assert(master.index() >= 0);
  const ShotMoment moment = Locate(project, master);
  const bool is_past_end = moment.shot < 0;
  if (is_past_end) {
    return Blank(project, scale, Qt::black);
  }
  const Shot& shot = *project.shots[static_cast<size_t>(moment.shot)];
  QImage frame = RenderShot(project, shot, moment.local, scale);
  const bool is_mixing = moment.next_shot >= 0;
  if (is_mixing) {
    const Shot& next = *project.shots[static_cast<size_t>(moment.next_shot)];
    frame = Combine(shot.transition.kind, moment.mix, frame,
                    RenderShot(project, next, moment.next_local, scale));
  }
  return frame;
}

QImage FrameRenderer::RenderReel(const Project& project, Frame frame,
                                 double scale, VideoFrames* videos) {
  assert(scale > 0.0);
  assert(frame.index() >= 0);
  QImage out = Blank(project, scale, Qt::black);
  for (const ReelPiece& piece : ReelAt(project, frame)) {
    const auto* video = std::get_if<VideoSource>(&piece.clip->source);
    const bool is_video = video != nullptr;
    if (is_video) {
      const QImage picture =
          videos != nullptr ? videos->Picture(video->path, piece.source)
                            : QImage();
      const bool has_picture = !picture.isNull();
      if (has_picture) {
        DrawFitted(picture, &out);
      }
      continue;
    }
    const Shot* shot =
        FindShot(project, std::get<ShotSource>(piece.clip->source).shot);
    assert(shot != nullptr);
    QPainter painter(&out);
    painter.drawImage(0, 0, RenderShot(project, *shot, piece.source, scale));
  }
  return out;
}

}  // namespace snapper
