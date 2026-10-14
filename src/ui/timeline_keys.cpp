#include "ui/timeline_keys.h"

#include <cassert>

namespace snapper {

bool ViewKey(const QKeyEvent& event, TimelineScale* scale,
             double playhead_frame) {
  assert(scale != nullptr);
  assert(event.type() == QEvent::KeyPress);
  const int key = event.key();
  const bool is_control = event.modifiers().testFlag(Qt::ControlModifier);
  const bool is_zoom_in =
      is_control && (key == Qt::Key_Equal || key == Qt::Key_Plus);
  const bool is_zoom_out = is_control && key == Qt::Key_Minus;
  const bool is_fit = is_control && key == Qt::Key_0;
  if (is_zoom_in) {
    scale->ZoomAroundFrame(kZoomStep, playhead_frame);
  } else if (is_zoom_out) {
    scale->ZoomAroundFrame(1.0 / kZoomStep, playhead_frame);
  } else if (is_fit) {
    scale->FitAll();
  }
  return is_zoom_in || is_zoom_out || is_fit;
}

int NudgeOf(const QKeyEvent& event) {
  assert(event.type() == QEvent::KeyPress);
  assert(event.key() != 0);
  const int key = event.key();
  const Qt::KeyboardModifiers mods = event.modifiers();
  const bool is_arrow = key == Qt::Key_Left || key == Qt::Key_Right;
  const bool is_nudge = is_arrow && mods.testFlag(Qt::AltModifier);
  if (!is_nudge) {
    return 0;
  }
  const int step = mods.testFlag(Qt::ShiftModifier) ? 10 : 1;
  return key == Qt::Key_Left ? -step : step;
}

}  // namespace snapper
