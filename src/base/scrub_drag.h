#ifndef SNAPPER_BASE_SCRUB_DRAG_H_
#define SNAPPER_BASE_SCRUB_DRAG_H_

namespace snapper {

// What a field's number can be, and how far one pixel of dragging moves
// it.
struct ScrubRange final {
  double unit = 1.0;
  int decimals = 0;
  double low = 0.0;
  double high = 0.0;
};

// The modifier keys held during a drag.
struct ScrubKeys final {
  bool shift = false;
  bool alt = false;
};

// The math of dragging a number's label. The drag is dead for the first
// kDeadZone pixels, so a click never changes anything; then the value
// jumps to where the cursor is and follows it, one unit per pixel.
// Shift makes a pixel count 10, Alt 0.1. Each move adds only the pixels
// since the last one, so pressing a key mid-drag never makes a jump.
class ScrubDrag final {
 public:
  static constexpr int kDeadZone = 4;

  ScrubDrag(double start, const ScrubRange& range);

  // The value for a cursor dx pixels from where the press began.
  double Move(int dx, ScrubKeys keys);

  bool is_active() const { return is_active_; }
  double start() const { return start_; }

 private:
  double start_;
  ScrubRange range_;
  double value_;
  int last_dx_ = 0;
  bool is_active_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_BASE_SCRUB_DRAG_H_
