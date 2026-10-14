#ifndef SNAPPER_UI_TIMELINE_SCALE_H_
#define SNAPPER_UI_TIMELINE_SCALE_H_

#include <QWheelEvent>
#include <QWidget>

namespace snapper {

// The zoom limits, in pixels per frame, the same on every timeline.
inline constexpr double kMinPixelsPerFrame = 0.02;
inline constexpr double kMaxPixelsPerFrame = 40.0;
// One wheel notch, or one Ctrl+= or Ctrl+-, zooms by this factor.
inline constexpr double kZoomStep = 1.25;
// One wheel notch scrolls this share of the frames in view.
inline constexpr double kScrollShare = 0.1;
// Pixels kept clear at the right when the whole length is fitted.
inline constexpr double kFitMargin = 8.0;

// What a wheel turn did.
enum class WheelUse {
  kZoomed,
  kScrolled,
  // Shift: the timeline's rows are the caller's to scroll, if it has
  // more than fit.
  kRows,
};

// Which frames a timeline shows and where: the one zoom, scroll, fit and
// follow rule of the Pose, Video and Comp timelines. Frames start left
// pixels in from the view's edge (the names column); it holds pixels per
// frame, the first frame in view and the length of what the timeline
// shows, and answers x for a frame and the frame at an x. The widget
// keeps its rows, clips and keys.
//
// Zoom is clamped to the shared limits and keeps the frame under the
// cursor (or the playhead) put. Fit shows the whole length and keeps
// showing it as the length changes, until the user zooms or scrolls.
// Following pages the view along a playing playhead: when it runs past
// the right edge (or back before the left) the view jumps to start at it;
// scrolling by hand stops that until the next play (BeginFollow).
class TimelineScale final {
 public:
  TimelineScale(const QWidget* view, double left, double pixels_per_frame);

  double left() const { return left_; }
  double pixels_per_frame() const;
  // The first frame in view (fractional).
  double first() const;
  // Frames that fit across the view at this zoom.
  double VisibleFrames() const;
  bool is_fit() const { return is_fit_; }
  int length() const { return length_; }

  // x of a frame's start, and the frame under x (fractional; negative
  // left of the frames when scrolled to the start).
  double XOf(double frame) const;
  double FrameAtX(double x) const;
  // The whole frame under x.
  int FrameAt(double x) const;

  // The frames the timeline shows (at least 1); scroll stays inside.
  void SetLength(int frames);
  // Zoom to pixels_per_frame (clamped), keeping the first frame.
  void SetPixelsPerFrame(double pixels_per_frame);
  // Shows factor times as large, keeping the frame under x put.
  void ZoomAround(double factor, double x);
  // Same, keeping frame where it is drawn now.
  void ZoomAroundFrame(double factor, double frame);
  // Moves the view by frames; scrolling by hand stops the following.
  void ScrollBy(double frames);
  // Shows the whole length.
  void FitAll();

  // Wheel: Ctrl zooms round the cursor, Shift hands the rows back, plain
  // scrolls time.
  WheelUse Wheel(const QWheelEvent& event);
  // Notches the wheel turned (some mice send a held key's wheel
  // sideways).
  static double Notches(const QWheelEvent& event);

  // A play started: the view follows the playhead again.
  void BeginFollow() { is_following_ = true; }
  bool is_following() const { return is_following_; }
  // Pages the view to show frame while following; true when it moved.
  bool Follow(double frame);

 private:
  double ViewWidth() const;
  // Leaves fit mode, keeping what is on screen.
  void Detach();
  void ClampFirst();

  const QWidget* view_;
  double left_;
  double pixels_per_frame_;
  double first_ = 0.0;
  int length_ = 1;
  bool is_fit_ = false;
  bool is_following_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_SCALE_H_
