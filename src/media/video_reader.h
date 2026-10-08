#ifndef SNAPPER_MEDIA_VIDEO_READER_H_
#define SNAPPER_MEDIA_VIDEO_READER_H_

#include <QImage>
#include <QSize>
#include <QString>
#include <QtClassHelperMacros>

#include "base/error.h"
#include "media/ffmpeg_handles.h"

namespace snapper {

// Three hours, the longest timeline Snapper has; longer files are
// refused.
constexpr int kMaxVideoSeconds = 3 * 60 * 60;

// What a video file holds, read without decoding any picture.
struct VideoInfo final {
  QSize size;
  double seconds = 0.0;
};

Result<VideoInfo> ProbeVideo(const QString& path);

// Reads the pictures of one video file by time. Reading forward, as
// playback and export do, decodes each picture once; jumping back or far
// ahead seeks first. Sound in the file is ignored. Not shared between
// threads: each thread opens its own.
class VideoReader final {
 public:
  VideoReader() = default;
  ~VideoReader() = default;
  Q_DISABLE_COPY_MOVE(VideoReader)

  Result<void> Open(const QString& path);
  bool IsOpen() const { return input_ != nullptr; }
  const QString& path() const { return path_; }
  const VideoInfo& info() const { return info_; }

  // The picture showing at seconds from the start: the last one that
  // starts at or before it. Before the first picture it is the first;
  // past the end it is the last.
  Result<QImage> PictureAt(double seconds);

 private:
  Result<void> SeekTo(double seconds);
  // Decodes the next picture into peek_; false at the end of the file.
  Result<bool> DecodeNext();
  double TimeOf(const AVFrame* frame) const;
  QImage ToImage(const AVFrame* frame);

  QString path_;
  VideoInfo info_;
  InputHandle input_;
  CodecHandle codec_;
  ScalerHandle scaler_;
  PacketHandle packet_;
  int stream_ = -1;
  double time_base_ = 0.0;
  double start_ = 0.0;
  // The picture last handed out (or about to be) and the one after it.
  FrameHandle shown_;
  FrameHandle peek_;
  double shown_time_ = 0.0;
  double peek_time_ = 0.0;
  bool has_shown_ = false;
  bool has_peek_ = false;
  bool is_flushed_ = false;
  bool is_ended_ = false;
  // The last picture converted, so a held frame is converted once.
  QImage image_;
  double image_time_ = -1.0;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_VIDEO_READER_H_
