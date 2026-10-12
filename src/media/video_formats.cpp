#include "media/video_formats.h"

#include <QPainter>

#include <cassert>

#include "base/text.h"
#include "media/ffmpeg_handles.h"

namespace snapper {
namespace {

// Pixels at least this see-through become GIF's clear color; the rest
// are laid over the shot color and made solid.
constexpr int kGifClearBelow = 1;

QImage Over(const QImage& frame, QColor under) {
  assert(!frame.isNull());
  assert(under.isValid());
  QImage out(frame.size(), QImage::Format_ARGB32_Premultiplied);
  out.fill(QColor(under.red(), under.green(), under.blue()));
  QPainter painter(&out);
  painter.drawImage(0, 0, frame);
  return out;
}

}  // namespace

bool KeepsAlpha(VideoFormat format) {
  return format != VideoFormat::kMp4;
}

bool HasSound(VideoFormat format) {
  return format != VideoFormat::kGif;
}

QString FormatSuffix(VideoFormat format) {
  switch (format) {
    case VideoFormat::kMp4:
      return QStringLiteral("mp4");
    case VideoFormat::kGif:
      return QStringLiteral("gif");
    case VideoFormat::kPngSequence:
      return QString();
    case VideoFormat::kWebm:
      return QStringLiteral("webm");
    case VideoFormat::kMov:
      return QStringLiteral("mov");
  }
  assert(false && "unknown format");
  return QString();
}

QString FormatName(VideoFormat format) {
  switch (format) {
    case VideoFormat::kMp4:
      return Tr("MP4");
    case VideoFormat::kGif:
      return Tr("GIF");
    case VideoFormat::kPngSequence:
      return Tr("PNG frames");
    case VideoFormat::kWebm:
      return Tr("WebM");
    case VideoFormat::kMov:
      return Tr("MOV");
  }
  assert(false && "unknown format");
  return QString();
}

QString WhyNoEncoder(VideoFormat format) {
  switch (format) {
    case VideoFormat::kMp4:
    case VideoFormat::kPngSequence:
      // MP4 falls back to MPEG-4; PNG frames are written by Qt.
      return QString();
    case VideoFormat::kGif:
      return avcodec_find_encoder(AV_CODEC_ID_GIF) != nullptr
                 ? QString()
                 : Tr("This FFmpeg has no GIF encoder.");
    case VideoFormat::kWebm:
      return avcodec_find_encoder_by_name("libvpx-vp9") != nullptr
                 ? QString()
                 : Tr("This FFmpeg has no VP9 encoder (libvpx-vp9), so it "
                      "can't write WebM. Try PNG frames or MOV.");
    case VideoFormat::kMov:
      return avcodec_find_encoder_by_name("prores_ks") != nullptr
                 ? QString()
                 : Tr("This FFmpeg has no ProRes encoder (prores_ks), so it "
                      "can't write MOV. Try PNG frames or WebM.");
  }
  assert(false && "unknown format");
  return QString();
}

QImage ReadyFor(VideoFormat format, const QImage& frame, QColor under) {
  assert(!frame.isNull());
  assert(under.isValid());
  const bool is_solid = !frame.hasAlphaChannel();
  if (is_solid) {
    return frame;
  }
  const bool is_gif = format == VideoFormat::kGif;
  if (!KeepsAlpha(format)) {
    return Over(frame, under);
  }
  if (!is_gif) {
    return frame;
  }
  // GIF: one clear color, everything else solid over the shot color.
  QImage solid = Over(frame, under).convertToFormat(QImage::Format_ARGB32);
  const QImage alpha = frame.convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < solid.height(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(solid.scanLine(y));
    const auto* seen = reinterpret_cast<const QRgb*>(alpha.constScanLine(y));
    for (int x = 0; x < solid.width(); ++x) {
      const bool is_clear = qAlpha(seen[x]) < kGifClearBelow;
      if (is_clear) {
        line[x] = qRgba(0, 0, 0, 0);
      }
    }
  }
  return solid;
}

}  // namespace snapper
