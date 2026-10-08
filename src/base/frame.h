#ifndef SNAPPER_BASE_FRAME_H_
#define SNAPPER_BASE_FRAME_H_

#include <compare>

namespace snapper {

// Film rate: poses are held on 2s and 3s.
constexpr int kFramesPerSecond = 24;
// Three hours, far past any song, so frame math never overflows.
constexpr int kMaxFrame = kFramesPerSecond * 60 * 60 * 3;

// A frame number, or a length in frames. Its own type so frames never
// mix with milliseconds or audio samples. Always 0 to kMaxFrame.
class Frame final {
 public:
  constexpr Frame() = default;
  constexpr explicit Frame(int index) : index_(Clamp(index)) {}

  constexpr int index() const { return index_; }
  constexpr auto operator<=>(const Frame&) const = default;

 private:
  static constexpr int Clamp(int index) {
    return index < 0 ? 0 : (index > kMaxFrame ? kMaxFrame : index);
  }

  int index_ = 0;
};

// Seconds exist only where media files are read or written. A frame
// covers the sound from its start, so this rounds down.
Frame FrameAtSeconds(double seconds);
double SecondsAtFrame(Frame frame);
// A length typed in seconds, as the nearest whole frame.
Frame FramesNearSeconds(double seconds);

}  // namespace snapper

#endif  // SNAPPER_BASE_FRAME_H_
