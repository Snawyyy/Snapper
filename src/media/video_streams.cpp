// The stream setup half of VideoEncoder: codecs and frame filling.

#include <cassert>
#include <cstring>

#include "base/frame.h"
#include "base/text.h"
#include "media/video_encoder.h"

namespace snapper {
namespace {

// GIF delays are in hundredths of a second.
constexpr int kGifTicks = 100;
constexpr int kPaletteSize = 256;

// The best H.264 encoder this FFmpeg has, falling back to MPEG-4 so an
// export never fails just for a missing codec.
const AVCodec* PickVideoCodec(VideoFormat format) {
  assert(format == VideoFormat::kMp4 || format == VideoFormat::kGif);
  const bool is_gif = format == VideoFormat::kGif;
  if (is_gif) {
    return avcodec_find_encoder(AV_CODEC_ID_GIF);
  }
  const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
  const bool has_x264 = codec != nullptr;
  if (!has_x264) {
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  }
  const bool has_h264 = codec != nullptr;
  if (!has_h264) {
    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  }
  assert(codec == nullptr || codec->type == AVMEDIA_TYPE_VIDEO);
  return codec;
}

Result<void> Ready(CodecHandle* handle, const AVCodec* codec,
                   AVFormatContext* output, AVStream** stream) {
  assert(handle != nullptr && stream != nullptr);
  assert(output != nullptr);
  const bool wants_global = output->oformat->flags & AVFMT_GLOBALHEADER;
  if (wants_global) {
    (*handle)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }
  *stream = avformat_new_stream(output, nullptr);
  const bool is_open = *stream != nullptr &&
                       avcodec_open2(handle->get(), codec, nullptr) >= 0 &&
                       avcodec_parameters_from_context((*stream)->codecpar,
                                                       handle->get()) >= 0;
  if (!is_open) {
    return std::unexpected(Error{Tr("This FFmpeg can't encode %1.")
                                     .arg(QLatin1String(codec->name))});
  }
  (*stream)->time_base = (*handle)->time_base;
  return {};
}

}  // namespace

Result<void> VideoEncoder::OpenVideo() {
  assert(output_ != nullptr);
  assert(settings_.width > 0 && settings_.height > 0);
  const AVCodec* codec = PickVideoCodec(settings_.format);
  const bool has_codec = codec != nullptr;
  if (!has_codec) {
    return std::unexpected(Error{Tr("This FFmpeg has no video encoder.")});
  }
  video_.reset(avcodec_alloc_context3(codec));
  const bool is_gif = settings_.format == VideoFormat::kGif;
  video_->width = settings_.width;
  video_->height = settings_.height;
  video_->time_base = is_gif ? AVRational{1, kGifTicks}
                             : AVRational{1, kFramesPerSecond};
  video_->framerate = AVRational{kFramesPerSecond, 1};
  video_->pix_fmt = is_gif ? AV_PIX_FMT_PAL8 : AV_PIX_FMT_YUV420P;
  // Near-lossless, so flat colours and hard edges stay clean.
  av_opt_set(video_->priv_data, "crf", "16", 0);
  av_opt_set(video_->priv_data, "preset", "medium", 0);
  auto ready = Ready(&video_, codec, output_.get(), &video_stream_);
  if (!ready) {
    return ready;
  }
  video_frame_.reset(av_frame_alloc());
  video_frame_->format = video_->pix_fmt;
  video_frame_->width = video_->width;
  video_frame_->height = video_->height;
  const bool has_buffer = av_frame_get_buffer(video_frame_.get(), 0) >= 0;
  scaler_.reset(is_gif ? nullptr
                       : sws_getContext(video_->width, video_->height,
                                        AV_PIX_FMT_RGB32, video_->width,
                                        video_->height, AV_PIX_FMT_YUV420P,
                                        SWS_BICUBIC, nullptr, nullptr,
                                        nullptr));
  const bool is_ready = has_buffer && (is_gif || scaler_ != nullptr);
  if (!is_ready) {
    return std::unexpected(Error{Tr("Export ran out of memory.")});
  }
  return {};
}

Result<void> VideoEncoder::OpenAudio() {
  assert(output_ != nullptr);
  assert(settings_.audio != nullptr);
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  const bool has_codec = codec != nullptr;
  if (!has_codec) {
    return std::unexpected(Error{Tr("This FFmpeg has no AAC encoder.")});
  }
  audio_.reset(avcodec_alloc_context3(codec));
  audio_->sample_fmt = AV_SAMPLE_FMT_FLTP;
  audio_->sample_rate = kAudioRate;
  audio_->bit_rate = 256000;
  audio_->time_base = AVRational{1, kAudioRate};
  av_channel_layout_default(&audio_->ch_layout, kAudioChannels);
  auto ready = Ready(&audio_, codec, output_.get(), &audio_stream_);
  if (!ready) {
    return ready;
  }
  audio_frame_.reset(av_frame_alloc());
  audio_frame_->format = audio_->sample_fmt;
  audio_frame_->sample_rate = kAudioRate;
  audio_frame_->nb_samples = audio_->frame_size;
  av_channel_layout_copy(&audio_frame_->ch_layout, &audio_->ch_layout);
  const bool has_buffer = audio_->frame_size > 0 &&
                          av_frame_get_buffer(audio_frame_.get(), 0) >= 0;
  if (!has_buffer) {
    return std::unexpected(Error{Tr("Export ran out of memory.")});
  }
  return {};
}

Result<void> VideoEncoder::FillVideoFrame(const QImage& image) {
  assert(!image.isNull());
  assert(video_frame_ != nullptr);
  const QSize size(settings_.width, settings_.height);
  const QImage sized =
      image.size() == size
          ? image
          : image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  const bool is_writable = av_frame_make_writable(video_frame_.get()) >= 0;
  if (!is_writable) {
    return std::unexpected(Error{Tr("Export ran out of memory.")});
  }
  const bool is_gif = settings_.format == VideoFormat::kGif;
  if (is_gif) {
    // Each frame gets its own palette; dithering hides the banding.
    const QImage indexed = sized.convertToFormat(
        QImage::Format_Indexed8, Qt::DiffuseDither | Qt::PreferDither);
    for (int y = 0; y < size.height(); ++y) {
      std::memcpy(video_frame_->data[0] + y * video_frame_->linesize[0],
                  indexed.constScanLine(y),
                  static_cast<size_t>(size.width()));
    }
    std::memset(video_frame_->data[1], 0, kPaletteSize * sizeof(uint32_t));
    const QList<QRgb> palette = indexed.colorTable();
    std::memcpy(video_frame_->data[1], palette.constData(),
                static_cast<size_t>(palette.size()) * sizeof(uint32_t));
    video_frame_->pts = frame_count_ * kGifTicks / kFramesPerSecond;
    return {};
  }
  const QImage rgb = sized.convertToFormat(QImage::Format_RGB32);
  const uint8_t* const planes[] = {rgb.constBits()};
  const int strides[] = {static_cast<int>(rgb.bytesPerLine())};
  sws_scale(scaler_.get(), planes, strides, 0, size.height(),
            video_frame_->data, video_frame_->linesize);
  video_frame_->pts = frame_count_;
  return {};
}

}  // namespace snapper
