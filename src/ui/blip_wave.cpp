#include "ui/blip_wave.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace snapper {
namespace {

constexpr int kRate = 22050;
constexpr int kMostMs = 2000;

void PutNumber(QByteArray* bytes, uint32_t value, int size) {
  assert(bytes != nullptr);
  assert(size == 2 || size == 4);
  for (int i = 0; i < size; ++i) {
    bytes->append(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

}  // namespace

QByteArray BlipWave(double start_hz, double end_hz, int ms, double quiet) {
  assert(start_hz > 0.0 && end_hz > 0.0);
  assert(quiet >= 0.0 && quiet <= 1.0);
  const int frames = kRate * std::clamp(ms, 1, kMostMs) / 1000;
  QByteArray wave;
  wave.append("RIFF");
  PutNumber(&wave, static_cast<uint32_t>(36 + frames * 2), 4);
  wave.append("WAVEfmt ");
  PutNumber(&wave, 16, 4);
  PutNumber(&wave, 1, 2);
  PutNumber(&wave, 1, 2);
  PutNumber(&wave, kRate, 4);
  PutNumber(&wave, kRate * 2, 4);
  PutNumber(&wave, 2, 2);
  PutNumber(&wave, 16, 2);
  wave.append("data");
  PutNumber(&wave, static_cast<uint32_t>(frames * 2), 4);
  double phase = 0.0;
  for (int i = 0; i < frames; ++i) {
    const double t = static_cast<double>(i) / frames;
    const double hz = start_hz + (end_hz - start_hz) * t;
    phase += 2.0 * std::numbers::pi * hz / kRate;
    const double fade = (1.0 - t) * (1.0 - t);
    const double sample = std::sin(phase) * fade * quiet * 32000.0;
    PutNumber(&wave, static_cast<uint16_t>(static_cast<int16_t>(sample)), 2);
  }
  return wave;
}

}  // namespace snapper
