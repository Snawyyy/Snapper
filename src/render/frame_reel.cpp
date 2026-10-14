// FrameRenderer: the reel, its clips on their tracks with their mixes,
// and the screen effects over the finished picture.

#include <QPainter>

#include <cassert>
#include <variant>

#include "anim/reel_timeline.h"
#include "render/frame_renderer.h"
#include "render/reel_text.h"

namespace snapper {
namespace {

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

// Mixes the two sides' shapes as Combine mixes their pictures, on
// clear: a swipe moves them, a fade or flash hands one to the other.
QImage CombineShapes(TransitionKind kind, double mix, const QImage& from,
                     const QImage& to) {
  assert(from.size() == to.size());
  assert(mix >= 0.0 && mix <= 1.0);
  QImage out(from.size(), QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter painter(&out);
  const QPointF way =
      kind == TransitionKind::kSwipeLeft    ? QPointF(-out.width(), 0)
      : kind == TransitionKind::kSwipeRight ? QPointF(out.width(), 0)
      : kind == TransitionKind::kSwipeUp    ? QPointF(0, -out.height())
      : kind == TransitionKind::kSwipeDown  ? QPointF(0, out.height())
                                            : QPointF();
  const bool is_swipe = !way.isNull();
  if (is_swipe) {
    painter.drawImage(way * mix, from);
    painter.drawImage(way * (mix - 1.0), to);
    return out;
  }
  const bool is_cut = kind == TransitionKind::kCut;
  const bool shows_to = is_cut || mix >= 0.5;
  painter.drawImage(0, 0, shows_to ? to : from);
  return out;
}

}  // namespace

void FrameRenderer::DrawClip(const Project& project, const Clip& clip,
                             Frame source, double scale, VideoFrames* videos,
                             QImage* out, QImage* drawn) {
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
  QImage shape;
  const bool wants_shape = drawn != nullptr;
  QPainter painter(out);
  painter.drawImage(0, 0,
                    RenderShot(project, *shot, source, scale, nullptr, false,
                               wants_shape ? &shape : nullptr));
  const bool has_shape = wants_shape && !shape.isNull();
  if (has_shape) {
    QPainter onto(drawn);
    onto.drawImage(0, 0, shape);
  }
}

QImage FrameRenderer::RenderReel(const Project& project, Frame frame,
                                 double scale, VideoFrames* videos,
                                 QColor under) {
  assert(scale > 0.0);
  assert(frame.index() >= 0);
  QImage out = Blank(project, scale, under);
  const bool needs_shape = NeedsReelShape(project, frame);
  QImage drawn =
      needs_shape ? Blank(project, scale, Qt::transparent) : QImage();
  QImage* shape = needs_shape ? &drawn : nullptr;
  for (const ReelPiece& piece : ReelAt(project, frame)) {
    // Text clips draw their own appear and leave; they never mix.
    const bool is_mixing = piece.next != nullptr && !IsTextClip(*piece.clip);
    if (!is_mixing) {
      DrawClip(project, *piece.clip, piece.source, scale, videos, &out,
               shape);
      continue;
    }
    // Both sides are drawn over what lies below, then mixed; their
    // shapes mix the same way, so effects on them move with a swipe.
    QImage from = out.copy();
    QImage from_shape = needs_shape ? drawn.copy() : QImage();
    DrawClip(project, *piece.clip, piece.source, scale, videos, &from,
             needs_shape ? &from_shape : nullptr);
    QImage to = out.copy();
    QImage to_shape = needs_shape ? drawn.copy() : QImage();
    DrawClip(project, *piece.next, piece.next_source, scale, videos, &to,
             needs_shape ? &to_shape : nullptr);
    out = Combine(piece.kind, piece.mix, from, to);
    if (needs_shape) {
      drawn = CombineShapes(piece.kind, piece.mix, from_shape, to_shape);
    }
  }
  ApplyReelComp(project, frame, scale, drawn, &out);
  return out;
}

}  // namespace snapper
