#ifndef SNAPPER_UI_GRAB_H_
#define SNAPPER_UI_GRAB_H_

#include <QPointF>
#include <Qt>

#include <cstdint>
#include <vector>

namespace snapper {

// How handles look and grab, one set for the stage, the rig canvas and
// the timelines. Handles look small and grab big.
//
// Handles are drawn this many pixels across.
inline constexpr double kHandleDraw = 8.0;
// A press this close to a handle grabs it; the nearest handle wins.
inline constexpr double kGrabReach = 12.0;
// A press turns into a drag only after the mouse goes this far; then
// the handle jumps to the cursor.
inline constexpr double kDragDeadZone = 4.0;
// A handle that just hid stays grabbable this long, so a click aimed at
// it a moment before it went still lands.
inline constexpr int kGraceMs = 100;
// A hovered handle grows by this factor.
inline constexpr double kHoverGrow = 1.15;

// How far along a hover is, 0 (not hovered) to 1 (fully grown), from
// the growth HandleHover reports; edges use it to widen and brighten.
double HoverAmount(double scale);

// What a handle does when dragged; the cursor says it before the press.
enum class GrabKind {
  kNone,
  kPivot,
  kIkTip,
  kLean,
  kSwivel,
  kWarpDot,
  kKey,
  kEdge,
  kColliderDot
};

// The cursor that shows what dragging this kind of handle does.
Qt::CursorShape CursorFor(GrabKind kind);

// Index of the spot nearest to point and within reach (the edge
// counts), or -1 when none is. A tie goes to the first spot.
int NearestGrab(const std::vector<QPointF>& spots, QPointF point,
                double reach = kGrabReach);

// True once the mouse has gone kDragDeadZone from where it was pressed.
bool IsPastDeadZone(QPointF press, QPointF now);

// Remembers when the handles of the picked things were last on show, so
// a press just after they hide can still grab them.
class HandleGrace final {
 public:
  // Call as the handles show (is_shown) or hide, with the clock in
  // milliseconds; only the change from shown to hidden starts the clock.
  void Update(bool is_shown, int64_t now_ms);
  // True while the handles are shown, or hid at most kGraceMs ago.
  bool IsGrabbable(bool is_shown, int64_t now_ms) const;

 private:
  bool was_shown_ = false;
  bool has_hidden_ = false;
  int64_t hidden_at_ms_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_GRAB_H_
