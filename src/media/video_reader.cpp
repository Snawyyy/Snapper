#include "media/video_reader.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

#include "base/frame.h"
#include "base/text.h"

namespace snapper {
namespace {

// FFmpeg's "no time" value, without its C-cast macro.
constexpr std::int64_t kNoTime = std::numeric_limits<std::int64_t>::min();
// Further ahead than this, seeking beats decoding every picture between.
constexpr double kSeekAheadSeconds = 3.0;
// Bounds every read loop: more packets than three hours of video has.
constexpr int kMaxPackets = 5'000'000;
// Pictures decoded for one request, enough to cross the longest gap
// between two keyframes any recorder makes.
constexpr int kMaxDecodesPerPicture = 100'000;

struct Opened final {
  InputHandle input;
  int stream = -1;
  const AVCodec* codec = nullptr;
};

Result<Opened> OpenInput(const QString& path) {
  assert(!path.isEmpty());
  assert(kMaxVideoSeconds > 0);
  Opened opened;
  AVFormatContext* raw = nullptr;
  const QByteArray name = path.toUtf8();
  const int code = avformat_open_input(&raw, name.constData(), nullptr,
                                       nullptr);
  const bool is_open = code >= 0;
  if (!is_open) {
    return std::unexpected(
        Error{Tr("Can't open %1: %2").arg(path, AvErrorText(code))});
  }
  opened.input.reset(raw);
  const bool has_info = avformat_find_stream_info(raw, nullptr) >= 0;
  opened.stream =
      av_find_best_stream(raw, AVMEDIA_TYPE_VIDEO, -1, -1, &opened.codec, 0);
  const bool has_video =
      has_info && opened.stream >= 0 && opened.codec != nullptr;
  if (!has_video) {
    return std::unexpected(Error{Tr("%1 has no video in it.").arg(path)});
  }
  return opened;
}

Result<VideoInfo> InfoOf(const QString& path, const Opened& opened) {
  assert(opened.input != nullptr);
  assert(opened.stream >= 0);
  const AVStream* stream = opened.input->streams[opened.stream];
  VideoInfo info;
  info.size = QSize(stream->codecpar->width, stream->codecpar->height);
  const bool has_own_length = stream->duration > 0;
  info.seconds = has_own_length
                     ? static_cast<double>(stream->duration) *
                           av_q2d(stream->time_base)
                     : static_cast<double>(opened.input->duration) /
                           AV_TIME_BASE;
  const bool is_usable = info.size.width() > 0 && info.size.height() > 0 &&
                         std::isfinite(info.seconds) && info.seconds > 0.0;
  if (!is_usable) {
    return std::unexpected(Error{Tr("%1 has no readable pictures.").arg(path)});
  }
  const bool is_too_long = info.seconds > kMaxVideoSeconds;
  if (is_too_long) {
    return std::unexpected(
        Error{Tr("%1 is longer than three hours.").arg(path)});
  }
  return info;
}

}  // namespace

Result<VideoInfo> ProbeVideo(const QString& path) {
  assert(!path.isEmpty());
  assert(path.size() < 100000);
  auto opened = OpenInput(path);
  if (!opened) {
    return std::unexpected(opened.error());
  }
  return InfoOf(path, *opened);
}

Result<void> VideoReader::Open(const QString& path) {
  assert(!path.isEmpty());
  assert(!IsOpen());
  auto opened = OpenInput(path);
  if (!opened) {
    return std::unexpected(opened.error());
  }
  auto info = InfoOf(path, *opened);
  if (!info) {
    return std::unexpected(info.error());
  }
  const AVStream* stream = opened->input->streams[opened->stream];
  codec_.reset(avcodec_alloc_context3(opened->codec));
  const bool is_ready =
      codec_ != nullptr &&
      avcodec_parameters_to_context(codec_.get(), stream->codecpar) >= 0;
  if (is_ready) {
    // Every core: a 4K speedpaint should still scrub.
    codec_->thread_count = 0;
  }
  const bool is_decoding =
      is_ready && avcodec_open2(codec_.get(), opened->codec, nullptr) >= 0;
  packet_.reset(av_packet_alloc());
  shown_.reset(av_frame_alloc());
  peek_.reset(av_frame_alloc());
  const bool has_memory =
      packet_ != nullptr && shown_ != nullptr && peek_ != nullptr;
  const bool can_decode = is_decoding && has_memory;
  if (!can_decode) {
    return std::unexpected(
        Error{Tr("Can't decode the video in %1.").arg(path)});
  }
  time_base_ = av_q2d(stream->time_base);
  start_ = stream->start_time == kNoTime
               ? 0.0
               : static_cast<double>(stream->start_time) * time_base_;
  stream_ = opened->stream;
  info_ = *info;
  path_ = path;
  input_ = std::move(opened->input);
  assert(IsOpen());
  return {};
}

Result<QImage> VideoReader::PictureAt(double seconds) {
  assert(IsOpen());
  assert(std::isfinite(seconds));
  const bool is_behind = has_shown_ && seconds < shown_time_;
  const double from = has_shown_ ? shown_time_ : 0.0;
  const bool is_far = seconds > from + kSeekAheadSeconds;
  const bool needs_seek = is_behind || is_far;
  if (needs_seek) {
    auto sought = SeekTo(seconds);
    if (!sought) {
      return std::unexpected(sought.error());
    }
  }
  for (int i = 0; i < kMaxDecodesPerPicture; ++i) {
    if (!has_peek_) {
      auto decoded = DecodeNext();
      if (!decoded) {
        return std::unexpected(decoded.error());
      }
      const bool is_end = !*decoded;
      if (is_end) {
        break;
      }
    }
    // The first picture shows even before its time.
    const bool is_due = !has_shown_ || peek_time_ <= seconds;
    if (!is_due) {
      break;
    }
    std::swap(shown_, peek_);
    shown_time_ = peek_time_;
    has_shown_ = true;
    has_peek_ = false;
  }
  if (!has_shown_) {
    return std::unexpected(
        Error{Tr("%1 has no readable pictures.").arg(path_)});
  }
  const bool is_new = shown_time_ != image_time_;
  if (is_new) {
    image_ = ToImage(shown_.get());
    image_time_ = image_.isNull() ? -1.0 : shown_time_;
  }
  const bool is_converted = !image_.isNull();
  if (!is_converted) {
    return std::unexpected(Error{Tr("Out of memory reading %1.").arg(path_)});
  }
  return image_;
}

Result<void> VideoReader::SeekTo(double seconds) {
  assert(IsOpen());
  assert(time_base_ > 0.0);
  const double target = std::max(seconds, 0.0) + start_;
  const auto stamp = static_cast<std::int64_t>(target / time_base_);
  const bool is_sought =
      av_seek_frame(input_.get(), stream_, stamp, AVSEEK_FLAG_BACKWARD) >= 0;
  if (!is_sought) {
    return std::unexpected(Error{Tr("Can't jump inside %1.").arg(path_)});
  }
  avcodec_flush_buffers(codec_.get());
  has_shown_ = false;
  has_peek_ = false;
  is_flushed_ = false;
  is_ended_ = false;
  return {};
}

Result<bool> VideoReader::DecodeNext() {
  assert(IsOpen());
  assert(!has_peek_);
  for (int i = 0; i < kMaxPackets && !is_ended_; ++i) {
    const int got = avcodec_receive_frame(codec_.get(), peek_.get());
    const bool has_picture = got >= 0;
    if (has_picture) {
      peek_time_ = TimeOf(peek_.get());
      has_peek_ = true;
      return true;
    }
    is_ended_ = IsEnd(got) || (is_flushed_ && IsAgain(got));
    const bool is_broken = !IsAgain(got) && !IsEnd(got);
    if (is_broken) {
      return std::unexpected(Error{Tr("%1 is damaged.").arg(path_)});
    }
    const bool can_feed = !is_ended_ && !is_flushed_;
    if (!can_feed) {
      continue;
    }
    const bool is_read = av_read_frame(input_.get(), packet_.get()) >= 0;
    if (!is_read) {
      // No more packets: let the decoder hand out what it still holds.
      avcodec_send_packet(codec_.get(), nullptr);
      is_flushed_ = true;
      continue;
    }
    const bool is_ours = packet_->stream_index == stream_;
    if (is_ours) {
      avcodec_send_packet(codec_.get(), packet_.get());
    }
    av_packet_unref(packet_.get());
  }
  return false;
}

double VideoReader::TimeOf(const AVFrame* frame) const {
  assert(frame != nullptr);
  assert(time_base_ > 0.0);
  const std::int64_t stamp = frame->best_effort_timestamp;
  const bool has_stamp = stamp != kNoTime;
  if (!has_stamp) {
    // No time on it: it comes right after the last one.
    return has_shown_ ? shown_time_ + 1.0 / kFramesPerSecond : 0.0;
  }
  return static_cast<double>(stamp) * time_base_ - start_;
}

QImage VideoReader::ToImage(const AVFrame* frame) {
  assert(frame != nullptr);
  assert(frame->width > 0 && frame->height > 0);
  const auto format = static_cast<AVPixelFormat>(frame->format);
  scaler_.reset(sws_getCachedContext(
      scaler_.release(), frame->width, frame->height, format, frame->width,
      frame->height, AV_PIX_FMT_RGB32, SWS_BILINEAR, nullptr, nullptr,
      nullptr));
  QImage image(frame->width, frame->height, QImage::Format_RGB32);
  const bool can_convert = scaler_ != nullptr && !image.isNull();
  if (!can_convert) {
    return QImage();
  }
  uint8_t* const planes[] = {image.bits()};
  const int strides[] = {static_cast<int>(image.bytesPerLine())};
  sws_scale(scaler_.get(), frame->data, frame->linesize, 0, frame->height,
            planes, strides);
  return image;
}

}  // namespace snapper
