#ifndef SNAPPER_MEDIA_AUDIO_CLIP_H_
#define SNAPPER_MEDIA_AUDIO_CLIP_H_

#include <QString>

#include <vector>

#include "base/error.h"

namespace snapper {

// Every song is turned into this one shape, so playback, waveform and
// export never deal with formats.
constexpr int kAudioRate = 48000;
constexpr int kAudioChannels = 2;
// Thirty minutes; longer files are refused rather than eat memory.
constexpr int kMaxAudioSeconds = 30 * 60;

// Decoded sound: 48 kHz stereo floats, left and right interleaved.
struct AudioClip final {
  std::vector<float> samples;

  int FrameCount() const {
    return static_cast<int>(samples.size() / kAudioChannels);
  }
  double Seconds() const {
    return static_cast<double>(FrameCount()) / kAudioRate;
  }
};

Result<AudioClip> DecodeAudio(const QString& path);

// The loudest sample in each of count equal slices, 0 to 1, for
// drawing a waveform.
std::vector<float> Peaks(const AudioClip& clip, int count);

// The loudest sample (0 to 1) between two times in seconds; 0 outside
// the clip. The timeline's waveform reads one per pixel column.
float PeakBetween(const AudioClip& clip, double from, double to);

// The part from start for length seconds, silent where the clip has
// nothing, so a range past the song's end still has its full length.
AudioClip Slice(const AudioClip& clip, double start, double length);

}  // namespace snapper

#endif  // SNAPPER_MEDIA_AUDIO_CLIP_H_
