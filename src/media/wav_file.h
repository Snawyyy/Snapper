#ifndef SNAPPER_MEDIA_WAV_FILE_H_
#define SNAPPER_MEDIA_WAV_FILE_H_

#include <QByteArray>

#include <cstdint>
#include <vector>

#include "media/audio_clip.h"

namespace snapper {

// A float sample (-1 to 1, clipped beyond) as a 16-bit one.
std::int16_t ShortSample(float sample);

// A whole 16-bit PCM WAV file: samples interleaved by channel.
QByteArray WaveFile(int rate, int channels,
                    const std::vector<std::int16_t>& samples);
// clip as a 48 kHz stereo 16-bit WAV file.
QByteArray WaveFile(const AudioClip& clip);

}  // namespace snapper

#endif  // SNAPPER_MEDIA_WAV_FILE_H_
