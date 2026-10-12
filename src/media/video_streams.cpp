// The stream setup half of VideoEncoder: codecs and frame filling.

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "base/frame.h"
#include "base/text.h"
#include "media/video_encoder.h"

namespace snapper {
namespace {

// GIF delays are in hundredths of a second.
constexpr int kGifTicks = 100;
constexpr int kPaletteSize = 256;

// PCM has no frame size of its own; this many samples go per frame.
constexpr int kPcmFrameSamples = 1024;

// The encoder for format. For MP4 the best H.264 one this FFmpeg has,
// falling back to MPEG-4 so an export never fails just for a missing
// codec; WhyNoEncoder refuses the others before this when missing.
const AVCodec* PickVideoCodec(VideoFormat format) {
  assert(format != VideoFormat::kPngSequence);
  switch (format) {
    case VideoFormat::kGif:
      return avcodec_find_encoder(AV_CODEC_ID_GIF);
    case VideoFormat::kWebm:
      return avcodec_find_encoder_by_name("libvpx-vp9");
    case VideoFormat::kMov:
      return avcodec_find_encoder_by_name("prores_ks");
    default:
      break;
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

AVPixelFormat PixelsFor(VideoFormat format) {
  switch (format) {
    case VideoFormat::kGif:
      return AV_PIX_FMT_PAL8;
    case VideoFormat::kWebm:
      return AV_PIX_FMT_YUVA420P;
    case VideoFormat::kMov:
      return AV_PIX_FMT_YUVA444P10LE;
    default:
      return AV_PIX_FMT_YUV420P;
  }
}

// The sound encoder for format and the sample layout it takes: AAC for
// MP4, Opus (else Vorbis) for WebM, 16-bit PCM for MOV, which editing
// programs read best.
struct AudioPick final {
  const AVCodec* codec = nullptr;
  AVSampleFormat samples = AV_SAMPLE_FMT_FLTP;
};

AudioPick PickAudioCodec(VideoFormat format) {
  assert(HasSound(format));
  switch (format) {
    case VideoFormat::kWebm: {
      const AVCodec* opus = avcodec_find_encoder_by_name("libopus");
      const bool has_opus = opus != nullptr;
      return has_opus ? AudioPick{opus, AV_SAMPLE_FMT_FLT}
                      : AudioPick{avcodec_find_encoder_by_name("libvorbis"),
                                  AV_SAMPLE_FMT_FLTP};
    }
    case VideoFormat::kMov:
      return {avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE), AV_SAMPLE_FMT_S16};
    default:
      return {avcodec_find_encoder(AV_CODEC_ID_AAC), AV_SAMPLE_FMT_FLTP};
  }
}

// Sets the format's quality options on a fresh codec context.
void SetQuality(VideoFormat format, AVCodecContext* codec) {
  assert(codec != nullptr);
  switch (format) {
    case VideoFormat::kWebm:
      // Constant quality, near-lossless for flat colours.
      codec->bit_rate = 0;
      av_opt_set(codec->priv_data, "crf", "18", 0);
      av_opt_set(codec->priv_data, "row-mt", "1", 0);
      av_opt_set(codec->priv_data, "cpu-used", "4", 0);
      break;
    case VideoFormat::kMov:
      av_opt_set(codec->priv_data, "profile", "4444", 0);
      av_opt_set(codec->priv_data, "vendor", "apl0", 0);
      break;
    default:
      // Near-lossless, so flat colours and hard edges stay clean.
      av_opt_set(codec->priv_data, "crf", "16", 0);
      av_opt_set(codec->priv_data, "preset", "medium", 0);
      break;
  }
}

bool HasClearPixel(const QImage& image) {
  assert(image.format() == QImage::Format_ARGB32);
  for (int y = 0; y < image.height(); ++y) {
    const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(line[x]) == 0) {
        return true;
      }
    }
  }
  return false;
}

// GIF's one clear color: the palette slot for fully clear pixels. Qt's
// palette for the solid ones may fill all 256 slots; then its least
// used colour gives way to the nearest other one.
void MakeClearSlot(const QImage& source, QImage* indexed) {
  assert(indexed != nullptr);
  assert(source.size() == indexed->size());
  QList<QRgb> palette = indexed->colorTable();
  std::vector<int> uses(static_cast<size_t>(kPaletteSize), 0);
  for (int y = 0; y < indexed->height(); ++y) {
    const uchar* line = indexed->constScanLine(y);
    for (int x = 0; x < indexed->width(); ++x) {
      ++uses[line[x]];
    }
  }
  int clear = static_cast<int>(palette.size());
  const bool is_full = clear >= kPaletteSize;
  if (is_full) {
    clear = static_cast<int>(
        std::min_element(uses.begin(), uses.end()) - uses.begin());
    int nearest = clear == 0 ? 1 : 0;
    int best = INT_MAX;
    for (int i = 0; i < kPaletteSize; ++i) {
      const QRgb a = palette[i];
      const QRgb b = palette[clear];
      const int distance = std::abs(qRed(a) - qRed(b)) +
                           std::abs(qGreen(a) - qGreen(b)) +
                           std::abs(qBlue(a) - qBlue(b));
      const bool is_nearer = i != clear && distance < best;
      if (is_nearer) {
        best = distance;
        nearest = i;
      }
    }
    for (int y = 0; y < indexed->height(); ++y) {
      uchar* line = indexed->scanLine(y);
      for (int x = 0; x < indexed->width(); ++x) {
        line[x] = line[x] == clear ? static_cast<uchar>(nearest) : line[x];
      }
    }
    palette[clear] = qRgba(0, 0, 0, 0);
  } else {
    palette.append(qRgba(0, 0, 0, 0));
  }
  indexed->setColorTable(palette);
  for (int y = 0; y < indexed->height(); ++y) {
    uchar* line = indexed->scanLine(y);
    const auto* seen = reinterpret_cast<const QRgb*>(source.constScanLine(y));
    for (int x = 0; x < indexed->width(); ++x) {
      line[x] = qAlpha(seen[x]) == 0 ? static_cast<uchar>(clear) : line[x];
    }
  }
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
  video_->pix_fmt = PixelsFor(settings_.format);
  SetQuality(settings_.format, video_.get());
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
                                        video_->height, video_->pix_fmt,
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
  const AudioPick pick = PickAudioCodec(settings_.format);
  const AVCodec* codec = pick.codec;
  const bool has_codec = codec != nullptr;
  if (!has_codec) {
    return std::unexpected(
        Error{Tr("This FFmpeg has no sound encoder for %1. Export without "
                 "the song, or pick another format.")
                  .arg(FormatName(settings_.format))});
  }
  audio_.reset(avcodec_alloc_context3(codec));
  audio_->sample_fmt = pick.samples;
  audio_->sample_rate = kAudioRate;
  const bool is_pcm = pick.samples == AV_SAMPLE_FMT_S16;
  if (!is_pcm) {
    audio_->bit_rate = 256000;
  }
  audio_->time_base = AVRational{1, kAudioRate};
  av_channel_layout_default(&audio_->ch_layout, kAudioChannels);
  auto ready = Ready(&audio_, codec, output_.get(), &audio_stream_);
  if (!ready) {
    return ready;
  }
  audio_frame_.reset(av_frame_alloc());
  audio_frame_->format = audio_->sample_fmt;
  audio_frame_->sample_rate = kAudioRate;
  audio_frame_->nb_samples =
      audio_->frame_size > 0 ? audio_->frame_size : kPcmFrameSamples;
  av_channel_layout_copy(&audio_frame_->ch_layout, &audio_->ch_layout);
  const bool has_buffer = av_frame_get_buffer(audio_frame_.get(), 0) >= 0;
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
          : image.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
  const bool is_writable = av_frame_make_writable(video_frame_.get()) >= 0;
  if (!is_writable) {
    return std::unexpected(Error{Tr("Export ran out of memory.")});
  }
  const bool is_gif = settings_.format == VideoFormat::kGif;
  if (is_gif) {
    // Each frame gets its own palette; dithering hides the banding.
    // ReadyFor left only fully clear pixels clear; the rest are solid.
    const QImage source = sized.convertToFormat(QImage::Format_ARGB32);
    QImage indexed =
        source.convertToFormat(QImage::Format_RGB32)
            .convertToFormat(QImage::Format_Indexed8,
                             Qt::DiffuseDither | Qt::PreferDither);
    const bool has_clear = HasClearPixel(source);
    if (has_clear) {
      MakeClearSlot(source, &indexed);
    }
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
  // Straight (not premultiplied) alpha, which the alpha formats keep
  // and the others drop.
  const QImage rgb = sized.convertToFormat(
      KeepsAlpha(settings_.format) ? QImage::Format_ARGB32
                                   : QImage::Format_RGB32);
  const uint8_t* const planes[] = {rgb.constBits()};
  const int strides[] = {static_cast<int>(rgb.bytesPerLine())};
  sws_scale(scaler_.get(), planes, strides, 0, size.height(),
            video_frame_->data, video_frame_->linesize);
  video_frame_->pts = frame_count_;
  return {};
}

}  // namespace snapper
