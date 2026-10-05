#include "media/audio_clip.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "base/text.h"
#include "media/ffmpeg_handles.h"

namespace snapper {
namespace {

constexpr size_t kMaxSamples =
    static_cast<size_t>(kMaxAudioSeconds) * kAudioRate * kAudioChannels;
// More packets than any 30-minute file has; bounds the read loop.
constexpr int kMaxPackets = 50'000'000;

struct Decoder final {
  InputHandle input;
  CodecHandle codec;
  ResamplerHandle resampler;
  int stream = -1;
};

Result<Decoder> OpenDecoder(const QString& path) {
  assert(!path.isEmpty());
  Decoder decoder;
  AVFormatContext* raw = nullptr;
  const QByteArray name = path.toUtf8();
  const int opened =
      avformat_open_input(&raw, name.constData(), nullptr, nullptr);
  const bool is_open = opened >= 0;
  if (!is_open) {
    return std::unexpected(
        Error{Tr("Can't open %1: %2").arg(path, AvErrorText(opened))});
  }
  decoder.input.reset(raw);
  const bool has_info = avformat_find_stream_info(raw, nullptr) >= 0;
  const AVCodec* codec = nullptr;
  decoder.stream =
      av_find_best_stream(raw, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  const bool has_audio = has_info && decoder.stream >= 0 && codec != nullptr;
  if (!has_audio) {
    return std::unexpected(Error{Tr("%1 has no sound in it.").arg(path)});
  }
  decoder.codec.reset(avcodec_alloc_context3(codec));
  const AVCodecParameters* parameters =
      raw->streams[decoder.stream]->codecpar;
  const bool is_ready =
      decoder.codec != nullptr &&
      avcodec_parameters_to_context(decoder.codec.get(), parameters) >= 0 &&
      avcodec_open2(decoder.codec.get(), codec, nullptr) >= 0;
  if (!is_ready) {
    return std::unexpected(Error{Tr("Can't decode the sound in %1.")
                                     .arg(path)});
  }
  assert(decoder.stream >= 0);
  return decoder;
}

Result<void> OpenResampler(Decoder* decoder) {
  assert(decoder != nullptr);
  assert(decoder->codec != nullptr);
  AVChannelLayout stereo{};
  av_channel_layout_default(&stereo, kAudioChannels);
  SwrContext* raw = nullptr;
  const AVCodecContext* codec = decoder->codec.get();
  const bool is_made =
      swr_alloc_set_opts2(&raw, &stereo, AV_SAMPLE_FMT_FLT, kAudioRate,
                          &codec->ch_layout, codec->sample_fmt,
                          codec->sample_rate, 0, nullptr) >= 0;
  decoder->resampler.reset(raw);
  const bool is_ready = is_made && swr_init(raw) >= 0;
  if (!is_ready) {
    return std::unexpected(Error{Tr("Can't convert this sound.")});
  }
  return {};
}

// Appends frame (or, with nullptr, what the resampler still holds).
bool Convert(SwrContext* resampler, const AVFrame* frame, AudioClip* clip) {
  assert(resampler != nullptr);
  assert(clip != nullptr);
  const int in_count = frame != nullptr ? frame->nb_samples : 0;
  const int room = swr_get_out_samples(resampler, in_count);
  const bool is_full = clip->samples.size() +
                           static_cast<size_t>(std::max(room, 0)) *
                               kAudioChannels > kMaxSamples;
  if (is_full) {
    return false;
  }
  const size_t at = clip->samples.size();
  clip->samples.resize(at + static_cast<size_t>(room) * kAudioChannels);
  auto* out = reinterpret_cast<uint8_t*>(clip->samples.data() + at);
  const int made = swr_convert(
      resampler, &out, room,
      frame != nullptr ? frame->extended_data : nullptr, in_count);
  clip->samples.resize(at + static_cast<size_t>(std::max(made, 0)) *
                                kAudioChannels);
  return made >= 0;
}

// Pulls every decoded frame out of the codec.
bool Drain(Decoder* decoder, AVFrame* frame, AudioClip* clip) {
  assert(decoder != nullptr && frame != nullptr);
  assert(clip != nullptr);
  for (int i = 0; i < kMaxPackets; ++i) {
    const int got = avcodec_receive_frame(decoder->codec.get(), frame);
    const bool is_waiting = got < 0;
    if (is_waiting) {
      return IsAgain(got) || IsEnd(got);
    }
    const bool is_kept = Convert(decoder->resampler.get(), frame, clip);
    av_frame_unref(frame);
    if (!is_kept) {
      return false;
    }
  }
  return false;
}

}  // namespace

Result<AudioClip> DecodeAudio(const QString& path) {
  assert(!path.isEmpty());
  auto decoder = OpenDecoder(path);
  const bool is_open = decoder.has_value() &&
                       OpenResampler(&*decoder).has_value();
  if (!is_open) {
    return std::unexpected(
        decoder ? Error{Tr("Can't convert the sound in %1.").arg(path)}
                : decoder.error());
  }
  const PacketHandle packet(av_packet_alloc());
  const FrameHandle frame(av_frame_alloc());
  AudioClip clip;
  bool is_fine = packet != nullptr && frame != nullptr;
  for (int i = 0; i < kMaxPackets && is_fine; ++i) {
    const bool is_end = av_read_frame(decoder->input.get(), packet.get()) < 0;
    if (is_end) {
      break;
    }
    const bool is_ours = packet->stream_index == decoder->stream;
    if (is_ours) {
      is_fine = avcodec_send_packet(decoder->codec.get(), packet.get()) >= 0 &&
                Drain(&*decoder, frame.get(), &clip);
    }
    av_packet_unref(packet.get());
  }
  const bool is_flushed =
      is_fine && avcodec_send_packet(decoder->codec.get(), nullptr) >= 0 &&
      Drain(&*decoder, frame.get(), &clip) &&
      Convert(decoder->resampler.get(), nullptr, &clip);
  if (!is_flushed) {
    return std::unexpected(Error{
        Tr("%1 is damaged or longer than 30 minutes.").arg(path)});
  }
  assert(clip.samples.size() % kAudioChannels == 0);
  return clip;
}

std::vector<float> Peaks(const AudioClip& clip, int count) {
  assert(count >= 0);
  assert(clip.samples.size() <= kMaxSamples);
  std::vector<float> peaks(static_cast<size_t>(count), 0.0f);
  const int frames = clip.FrameCount();
  const bool is_empty = frames == 0 || count == 0;
  if (is_empty) {
    return peaks;
  }
  for (int i = 0; i < frames; ++i) {
    const auto bucket = static_cast<size_t>(
        static_cast<std::int64_t>(i) * count / frames);
    for (int c = 0; c < kAudioChannels; ++c) {
      const float level = std::abs(
          clip.samples[static_cast<size_t>(i * kAudioChannels + c)]);
      peaks[bucket] = std::max(peaks[bucket], std::min(level, 1.0f));
    }
  }
  return peaks;
}

AudioClip Slice(const AudioClip& clip, double start, double length) {
  assert(std::isfinite(start) && std::isfinite(length));
  assert(length >= 0.0 && length <= kMaxAudioSeconds);
  const auto first = static_cast<std::int64_t>(std::lround(
                         start * kAudioRate)) * kAudioChannels;
  const auto count = static_cast<std::int64_t>(std::lround(
                         length * kAudioRate)) * kAudioChannels;
  AudioClip slice;
  slice.samples.assign(static_cast<size_t>(count), 0.0f);
  const auto size = static_cast<std::int64_t>(clip.samples.size());
  for (std::int64_t i = 0; i < count; ++i) {
    const std::int64_t from = first + i;
    const bool is_inside = from >= 0 && from < size;
    if (is_inside) {
      slice.samples[static_cast<size_t>(i)] =
          clip.samples[static_cast<size_t>(from)];
    }
  }
  return slice;
}

}  // namespace snapper
