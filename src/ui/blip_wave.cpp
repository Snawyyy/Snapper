#include "ui/blip_wave.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "media/wav_file.h"

namespace snapper {
namespace {

constexpr int kRate = 22050;
constexpr int kMostMs = 2000;

}  // namespace

QByteArray BlipWave(double start_hz, double end_hz, int ms, double quiet) {
  assert(start_hz > 0.0 && end_hz > 0.0);
  assert(quiet >= 0.0 && quiet <= 1.0);
  const int frames = kRate * std::clamp(ms, 1, kMostMs) / 1000;
  std::vector<std::int16_t> samples;
  samples.reserve(static_cast<size_t>(frames));
  double phase = 0.0;
  for (int i = 0; i < frames; ++i) {
    const double t = static_cast<double>(i) / frames;
    const double hz = start_hz + (end_hz - start_hz) * t;
    phase += 2.0 * std::numbers::pi * hz / kRate;
    const double fade = (1.0 - t) * (1.0 - t);
    const double sample = std::sin(phase) * fade * quiet * 32000.0;
    samples.push_back(static_cast<std::int16_t>(sample));
  }
  return WaveFile(kRate, 1, samples);
}

}  // namespace snapper
