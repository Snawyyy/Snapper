#include "media/video_encoder.h"

#include <QFile>

#include <algorithm>
#include <cassert>

#include "base/frame.h"
#include "base/text.h"

namespace snapper {
namespace {

// More packets than one frame ever makes; bounds the drain loop.
constexpr int kMaxPacketsPerSend = 4096;
// A 3-hour video at 24 fps; bounds the audio and flush loops.
constexpr std::int64_t kMaxFrames = kMaxFrame;

Error Failed(const char* what, int code) {
  assert(what != nullptr);
  assert(code < 0);
  return Error{Tr("Export failed while %1: %2")
                   .arg(Tr(what), AvErrorText(code))};
}

}  // namespace

VideoEncoder::~VideoEncoder() {
  assert(frame_count_ >= 0);
  const bool is_half_written = is_open_ && !is_finished_;
  output_.reset();
  if (is_half_written) {
    QFile::remove(settings_.path);
  }
  assert(output_ == nullptr);
}

Result<void> VideoEncoder::Open(const EncodeSettings& settings) {
  assert(!is_open_);
  assert(!settings.path.isEmpty());
  settings_ = settings;
  const char* muxer = settings.format == VideoFormat::kGif ? "gif" : "mp4";
  AVFormatContext* raw = nullptr;
  const QByteArray path = settings.path.toUtf8();
  const int made =
      avformat_alloc_output_context2(&raw, nullptr, muxer, path.constData());
  const bool is_made = made >= 0 && raw != nullptr;
  if (!is_made) {
    return std::unexpected(Failed("starting the file", made));
  }
  output_.reset(raw);
  packet_.reset(av_packet_alloc());
  auto video = OpenVideo();
  if (!video) {
    return video;
  }
  const bool wants_audio =
      settings.format == VideoFormat::kMp4 && settings.audio != nullptr;
  if (wants_audio) {
    auto audio = OpenAudio();
    if (!audio) {
      return audio;
    }
  }
  const int opened = avio_open(&raw->pb, path.constData(), AVIO_FLAG_WRITE);
  const bool is_writable = opened >= 0;
  if (!is_writable) {
    return std::unexpected(Failed("opening the file", opened));
  }
  is_open_ = true;
  const int header = avformat_write_header(raw, nullptr);
  const bool has_header = header >= 0;
  if (!has_header) {
    return std::unexpected(Failed("writing the header", header));
  }
  return {};
}

Result<void> VideoEncoder::AddFrame(const QImage& frame) {
  assert(is_open_ && !is_finished_);
  assert(!frame.isNull());
  auto filled = FillVideoFrame(frame);
  if (!filled) {
    return filled;
  }
  auto sent = Send(video_.get(), video_stream_, video_frame_.get());
  if (!sent) {
    return sent;
  }
  ++frame_count_;
  // Keep sound level with the picture so the file interleaves well.
  return EncodeAudioUntil(frame_count_ * kAudioRate / kFramesPerSecond);
}

Result<void> VideoEncoder::Finish() {
  assert(is_open_);
  assert(!is_finished_);
  const std::int64_t clip_end =
      audio_ != nullptr ? settings_.audio->FrameCount() : 0;
  auto audio = EncodeAudioUntil(clip_end);
  if (!audio) {
    return audio;
  }
  auto video = Send(video_.get(), video_stream_, nullptr);
  if (!video) {
    return video;
  }
  const bool has_audio = audio_ != nullptr;
  if (has_audio) {
    auto flushed = Send(audio_.get(), audio_stream_, nullptr);
    if (!flushed) {
      return flushed;
    }
  }
  const int trailer = av_write_trailer(output_.get());
  const bool is_written = trailer >= 0;
  if (!is_written) {
    return std::unexpected(Failed("finishing the file", trailer));
  }
  is_finished_ = true;
  return {};
}

Result<void> VideoEncoder::EncodeAudioUntil(std::int64_t sample) {
  assert(sample >= 0);
  assert(audio_cursor_ >= 0);
  const bool has_audio = audio_ != nullptr;
  if (!has_audio) {
    return {};
  }
  const AudioClip& clip = *settings_.audio;
  const int size = audio_->frame_size;
  const std::int64_t end = std::min<std::int64_t>(sample, clip.FrameCount());
  for (std::int64_t step = 0; step < kMaxFrames && audio_cursor_ < end;
       ++step) {
    const bool is_writable = av_frame_make_writable(audio_frame_.get()) >= 0;
    if (!is_writable) {
      return std::unexpected(Error{Tr("Export ran out of memory.")});
    }
    auto* left = reinterpret_cast<float*>(audio_frame_->data[0]);
    auto* right = reinterpret_cast<float*>(audio_frame_->data[1]);
    for (int i = 0; i < size; ++i) {
      const std::int64_t at = audio_cursor_ + i;
      const bool is_inside = at < clip.FrameCount();
      const auto index = static_cast<size_t>(at * kAudioChannels);
      left[i] = is_inside ? clip.samples[index] : 0.0f;
      right[i] = is_inside ? clip.samples[index + 1] : 0.0f;
    }
    audio_frame_->pts = audio_cursor_;
    audio_cursor_ += size;
    auto sent = Send(audio_.get(), audio_stream_, audio_frame_.get());
    if (!sent) {
      return sent;
    }
  }
  return {};
}

Result<void> VideoEncoder::Send(AVCodecContext* codec, AVStream* stream,
                                const AVFrame* frame) {
  assert(codec != nullptr && stream != nullptr);
  assert(packet_ != nullptr);
  const int sent = avcodec_send_frame(codec, frame);
  const bool is_sent = sent >= 0;
  if (!is_sent) {
    return std::unexpected(Failed("encoding", sent));
  }
  for (int i = 0; i < kMaxPacketsPerSend; ++i) {
    const int got = avcodec_receive_packet(codec, packet_.get());
    const bool is_drained = IsAgain(got) || IsEnd(got);
    if (is_drained) {
      return {};
    }
    const bool is_broken = got < 0;
    if (is_broken) {
      return std::unexpected(Failed("encoding", got));
    }
    av_packet_rescale_ts(packet_.get(), codec->time_base, stream->time_base);
    packet_->stream_index = stream->index;
    const int written =
        av_interleaved_write_frame(output_.get(), packet_.get());
    const bool is_written = written >= 0;
    if (!is_written) {
      return std::unexpected(Failed("writing", written));
    }
  }
  return std::unexpected(Error{Tr("Export got stuck in the encoder.")});
}

}  // namespace snapper
