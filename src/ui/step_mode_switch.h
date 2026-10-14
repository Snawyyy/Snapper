#ifndef SNAPPER_UI_STEP_MODE_SWITCH_H_
#define SNAPPER_UI_STEP_MODE_SWITCH_H_

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QRect>
#include <QToolButton>
#include <QWidget>

#include "edit/playback_manager.h"

namespace snapper {

// How the arrow keys step, as two icons side by side: a film frame
// (scrub: one frame at a time) and a diamond (animation: jump to the
// next frame where something changes). The one in use shows red.
class StepModeSwitch final : public QWidget {
  Q_OBJECT

 public:
  StepModeSwitch(PlaybackManager* playback, QWidget* parent);

  // Where it sits on every timeline (Pose, Video, Comp): the ruler's
  // top-left corner, as wide as the names column, at most kNameWidth.
  static QRect CornerRect(int names_width);

 private:
  void Show();

  PlaybackManager* playback_;
  QHBoxLayout layout_;
  QButtonGroup group_;
  QToolButton scrub_;
  QToolButton animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STEP_MODE_SWITCH_H_
