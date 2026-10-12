#include "media/wav_file.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

constexpr int kSampleBytes = 2;
constexpr int kHeaderRest = 36;
constexpr int kFormatChunk = 16;
constexpr int kPcm = 1;
constexpr float kFullScale = 32767.0f;

void PutNumber(QByteArray* bytes, std::uint32_t value, int size) {
  assert(bytes != nullptr);
  assert(size == 2 || size == 4);
  for (int i = 0; i < size; ++i) {
    bytes->append(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

}  // namespace

std::int16_t ShortSample(float sample) {
  return static_cast<std::int16_t>(
      std::lround(std::clamp(sample, -1.0f, 1.0f) * kFullScale));
}

QByteArray WaveFile(int rate, int channels,
                    const std::vector<std::int16_t>& samples) {
  assert(rate > 0);
  assert(channels > 0);
  const auto data = static_cast<std::uint32_t>(samples.size() * kSampleBytes);
  const auto block = static_cast<std::uint32_t>(channels * kSampleBytes);
  QByteArray wave;
  wave.reserve(static_cast<qsizetype>(data) + kHeaderRest + 8);
  wave.append("RIFF");
  PutNumber(&wave, kHeaderRest + data, 4);
  wave.append("WAVEfmt ");
  PutNumber(&wave, kFormatChunk, 4);
  PutNumber(&wave, kPcm, 2);
  PutNumber(&wave, static_cast<std::uint32_t>(channels), 2);
  PutNumber(&wave, static_cast<std::uint32_t>(rate), 4);
  PutNumber(&wave, static_cast<std::uint32_t>(rate) * block, 4);
  PutNumber(&wave, block, 2);
  PutNumber(&wave, kSampleBytes * 8, 2);
  wave.append("data");
  PutNumber(&wave, data, 4);
  for (const std::int16_t sample : samples) {
    PutNumber(&wave, static_cast<std::uint16_t>(sample), 2);
  }
  return wave;
}

QByteArray WaveFile(const AudioClip& clip) {
  assert(clip.samples.size() % kAudioChannels == 0);
  std::vector<std::int16_t> samples;
  samples.reserve(clip.samples.size());
  for (const float sample : clip.samples) {
    samples.push_back(ShortSample(sample));
  }
  return WaveFile(kAudioRate, kAudioChannels, samples);
}

}  // namespace snapper
