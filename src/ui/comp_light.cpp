#include "ui/comp_light.h"

#include <cassert>

#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/angle_dial.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"

namespace snapper {

CompLight::CompLight(const Managers& managers)
    : managers_(managers),
      live_(managers.history),
      layout_(this),
      key_(tr("Key")) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  box_.setRange(0.0, 360.0);
  box_.setDecimals(0);
  box_.setWrapping(true);
  box_.setSuffix(tr(" deg"));
  box_.setToolTip(tr("Where the light comes from (0 up, clockwise), for "
                     "every rim light and gradient that follows the shot "
                     "light."));
  layout_.addWidget(MakeOwned<AngleDial>(&box_, &live_, this, 36));
  layout_.addWidget(&box_);
  key_.setCheckable(true);
  key_.setFocusPolicy(Qt::NoFocus);
  key_.setToolTip(tr("Key the shot light at the playhead, so a light can "
                     "swing across the shot."));
  layout_.addWidget(&key_);
  layout_.addStretch();
  MakeLive(&box_, &live_, tr("Shot light"), this, [this] {
    Report(ProblemOf(managers_.comp->SetShotLight(
        managers_.selection->shot(), Local(), box_.value())));
  });
  connect(&key_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.comp->ToggleShotLightKey(
        managers_.selection->shot(), Local())));
  });
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  Refresh();
}

Frame CompLight::Local() const {
  assert(managers_.playback != nullptr);
  assert(managers_.history != nullptr);
  const ReelShot at = ShotAtReel(managers_.history->current(),
                                 managers_.playback->FrameOn(Timeline::kReel));
  return at.IsValid() ? at.local : Frame(0);
}

void CompLight::Refresh() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const Frame local = Local();
  const bool can_show = !box_.hasFocus();
  if (can_show) {
    ShowNumber(&box_, managers_.comp->ShotLight(shot, local));
  }
  key_.setChecked(managers_.comp->HasShotLightKeyAt(shot, local));
}

}  // namespace snapper
