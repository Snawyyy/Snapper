#ifndef SNAPPER_MEDIA_VIDEO_ENCODER_H_
#define SNAPPER_MEDIA_VIDEO_ENCODER_H_

#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QtClassHelperMacros>

#include <cstdint>
#include <memory>

#include "base/error.h"
#include "media/audio_clip.h"
#include "media/ffmpeg_handles.h"
#include "media/video_formats.h"

namespace snapper {

struct EncodeSettings final {
  // A file, or for a PNG sequence the folder the frames go in.
  QString path;
  VideoFormat format = VideoFormat::kMp4;
  int width = 1920;
  int height = 1080;
  // The sound for exactly the exported range; nullptr for a silent
  // video. Ignored for GIF (see HasSound).
  std::shared_ptr<const AudioClip> audio;
};

// Writes frames at 24 fps: MP4 (H.264 and AAC), GIF, WebM (VP9 with
// alpha, Opus or Vorbis), MOV (ProRes 4444 with alpha, PCM sound) or a
// folder of PNG frames (frame_00000.png on, the sound as sound.wav). An
// encoder dropped before Finish deletes what it half wrote, so a
// cancelled or failed export leaves nothing behind.
class VideoEncoder final {
 public:
  VideoEncoder() = default;
  ~VideoEncoder();
  Q_DISABLE_COPY_MOVE(VideoEncoder)

  Result<void> Open(const EncodeSettings& settings);
  // frame is scaled to the video size if it differs. Where it is
  // see-through, a format that can't keep that shows under (ReadyFor).
  Result<void> AddFrame(const QImage& frame, QColor under = Qt::black);
  // Writes the rest of the sound and closes the file.
  Result<void> Finish();

 private:
  Result<void> OpenPngs();
  Result<void> AddPng(const QImage& frame);
  Result<void> FinishPngs();
  void RemovePngs();
  Result<void> OpenVideo();
  Result<void> OpenAudio();
  Result<void> FillVideoFrame(const QImage& image);
  void FillAudioFrame(int size);
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
  // PNG sequence: the files written so far, and whether Open made the
  // folder, so a cancel can take them away again.
  QStringList written_;
  bool made_folder_ = false;
  bool is_open_ = false;
  bool is_finished_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_VIDEO_ENCODER_H_
