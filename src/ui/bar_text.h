#ifndef SNAPPER_UI_BAR_TEXT_H_
#define SNAPPER_UI_BAR_TEXT_H_

#include <QString>

#include "base/frame.h"

namespace snapper {

// What an Undo or Redo control shows: the words, the tooltip and
// whether it can act. One value feeds the menu entry and the top bar
// button, so they never disagree.
struct StepFace final {
  QString text;
  QString tooltip;
  bool enabled = false;
};

// "Undo Rotate head" when why_not is empty, else just "Undo", greyed
// out with why_not as its tooltip.
StepFace MakeStepFace(const QString& verb, const QString& label,
                      const QString& why_not);

// "F 024": the playhead's frame, three digits at least.
QString FrameCounterText(Frame frame);
// "0:01.00": minutes, seconds and the frame within the second.
QString TimecodeText(Frame frame);
// What the play button says right now.
QString PlayText(bool is_playing);

}  // namespace snapper

#endif  // SNAPPER_UI_BAR_TEXT_H_
