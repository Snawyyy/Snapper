#ifndef SNAPPER_UI_BLIP_WAVE_H_
#define SNAPPER_UI_BLIP_WAVE_H_

#include <QByteArray>

namespace snapper {

// A short soft tone as a WAV file (16-bit, mono): a sine that starts at
// start_hz and glides to end_hz over ms milliseconds, fading out, at
// most quiet (0 to 1) of full volume.
QByteArray BlipWave(double start_hz, double end_hz, int ms, double quiet);

}  // namespace snapper

#endif  // SNAPPER_UI_BLIP_WAVE_H_
