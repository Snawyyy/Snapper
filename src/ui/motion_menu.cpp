#include "ui/motion_menu.h"

#include <array>
#include <cassert>
#include <utility>

#include "ui/motion.h"

namespace snapper {

MotionMenu::MotionMenu(MotionManager* motion)
    : motion_(motion),
      menu_(tr("&Motion")),
      group_(this),
      full_action_(tr("&Full"), this),
      calm_action_(tr("&Calm"), this),
      off_action_(tr("&Off"), this) {
  assert(motion_ != nullptr);
  menu_.setToolTipsVisible(true);
  full_action_.setToolTip(tr("Springy pops and full-length moves."));
  calm_action_.setToolTip(tr("Shorter moves and no overshoot."));
  off_action_.setToolTip(tr("Everything snaps; nothing animates."));
  const std::array<std::pair<QAction*, MotionMode>, 3> choices{{
      {&full_action_, MotionMode::kFull},
      {&calm_action_, MotionMode::kCalm},
      {&off_action_, MotionMode::kOff},
  }};
  for (const auto& [action, mode] : choices) {
    action->setCheckable(true);
    group_.addAction(action);
    menu_.addAction(action);
    connect(action, &QAction::triggered, this,
            [this, mode] { motion_->SetMode(mode); });
  }
  connect(motion_, &MotionManager::ModeChanged, this,
          [this](MotionMode mode) { Show(mode); });
  Show(motion_->mode());
  assert(group_.checkedAction() != nullptr);
}

void MotionMenu::Show(MotionMode mode) {
  assert(mode >= MotionMode::kFull && mode <= MotionMode::kOff);
  assert(motion_ != nullptr);
  const bool is_full = mode == MotionMode::kFull;
  const bool is_calm = mode == MotionMode::kCalm;
  full_action_.setChecked(is_full);
  calm_action_.setChecked(is_calm);
  off_action_.setChecked(!is_full && !is_calm);
}

}  // namespace snapper
