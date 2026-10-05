#ifndef SNAPPER_MEDIA_VIDEO_ENCODER_H_
#define SNAPPER_MEDIA_VIDEO_ENCODER_H_

#include <QImage>
#include <QString>
#include <QtClassHelperMacros>

#include <cstdint>
#include <memory>

#include "base/error.h"
#include "media/audio_clip.h"
#include "media/ffmpeg_handles.h"

namespace snapper {

enum class VideoFormat { kMp4, kGif };

struct EncodeSettings final {
  QString path;
  VideoFormat format = VideoFormat::kMp4;
  int width = 1920;
  int height = 1080;
  // MP4 only: the sound for exactly the exported range; nullptr for a
  // silent video. GIF has no sound.
  std::shared_ptr<const AudioClip> audio;
};

// Writes frames to an MP4 (H.264 and AAC) or a GIF at 24 fps. An
// encoder dropped before Finish deletes its half-written file, so a
// cancelled or failed export leaves nothing behind.
class VideoEncoder final {
 public:
  VideoEncoder() = default;
  ~VideoEncoder();
  Q_DISABLE_COPY_MOVE(VideoEncoder)

  Result<void> Open(const EncodeSettings& settings);
  // frame is scaled to the video size if it differs.
  Result<void> AddFrame(const QImage& frame);
  // Writes the rest of the sound and closes the file.
  Result<void> Finish();

 private:
  Result<void> OpenVideo();
  Result<void> OpenAudio();
  Result<void> FillVideoFrame(const QImage& image);
  Result<void> EncodeAudioUntil(std::int64_t sample);
  Result<void> Send(AVCodecContext* codec, AVStream* stream,
                    const AVFrame* frame);

  EncodeSettings settings_;
  OutputHandle output_;
  CodecHandle video_;
  CodecHandle audio_;
  AVStream* video_stream_ = nullptr;
  AVStream* audio_stream_ = nullptr;
  ScalerHandle scaler_;
  FrameHandle video_frame_;
  FrameHandle audio_frame_;
  PacketHandle packet_;
  std::int64_t frame_count_ = 0;
  std::int64_t audio_cursor_ = 0;
  bool is_open_ = false;
  bool is_finished_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_VIDEO_ENCODER_H_
