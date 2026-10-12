#ifndef SNAPPER_MEDIA_VIDEO_FORMATS_H_
#define SNAPPER_MEDIA_VIDEO_FORMATS_H_

#include <QColor>
#include <QImage>
#include <QString>

namespace snapper {

// What an export writes. MP4 and GIF are the everyday ones; the PNG
// sequence (a folder of numbered frames), WebM (VP9) and MOV (ProRes
// 4444) keep transparency for compositing elsewhere.
enum class VideoFormat { kMp4, kGif, kPngSequence, kWebm, kMov };
constexpr int kVideoFormatCount = 5;

// True when see-through pixels stay see-through. GIF counts: it has
// one see-through color, soft edges are blended over the shot color.
bool KeepsAlpha(VideoFormat format);
// True when the song goes along: inside the file, or for a PNG
// sequence as a WAV next to the frames.
bool HasSound(VideoFormat format);
// The file name ending, without the dot; empty for a PNG sequence,
// which is a folder.
QString FormatSuffix(VideoFormat format);
// Short name for the format row, e.g. "WebM".
QString FormatName(VideoFormat format);
// Why this FFmpeg can't write format (an encoder is missing); empty
// when it can.
QString WhyNoEncoder(VideoFormat format);

// frame (premultiplied, maybe see-through) made ready for format:
// formats without alpha get it laid over under; GIF gets soft edges
// blended over under and only fully clear pixels left clear; the
// others keep it as it is.
QImage ReadyFor(VideoFormat format, const QImage& frame, QColor under);

}  // namespace snapper

#endif  // SNAPPER_MEDIA_VIDEO_FORMATS_H_
