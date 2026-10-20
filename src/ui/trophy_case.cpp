#include "ui/trophy_case.h"

#include <cassert>

#include "edit/achievement_manager.h"
#include "ui/achievements_window.h"
#include "ui/character_session.h"
#include "ui/stage_view.h"
#include "ui/toast_manager.h"

namespace snapper {

TrophyCase::TrophyCase(const Managers& managers, StageView* stage,
                       CharacterSession* session, QWidget* window)
    : managers_(managers),
      window_(window),
      open_(tr("&Achievements...")) {
  assert(stage != nullptr && session != nullptr);
  assert(window_ != nullptr);
  AchievementManager* trophies = managers_.achievements;
  open_.setEnabled(trophies != nullptr);
  open_.setToolTip(trophies != nullptr
                       ? tr("Your trophy case and your numbers.")
                       : tr("Achievements are off in this window."));
  connect(&open_, &QAction::triggered, this, &TrophyCase::Open);
  if (trophies == nullptr) {
    return;
  }
  connect(trophies, &AchievementManager::Earned, this, [this](int index) {
    managers_.toasts->Achieve(tr("Achievement complete! %1: %2")
                                  .arg(tr(kAchievements[index].name),
                                       tr(kAchievements[index].how)));
  });
  connect(stage, &StageView::LightTableChanged, trophies,
          [trophies](bool is_on) {
            if (is_on) {
              trophies->Count(AchievementStat::kLightTable);
            }
          });
  connect(session, &CharacterSession::Changed, trophies,
          [trophies, session] {
            trophies->UseTool(static_cast<int>(session->tool()));
          });
  connect(managers_.toasts, &ToastManager::Dismissed, trophies,
          [trophies] { trophies->Count(AchievementStat::kNotesDismissed); });
}

void TrophyCase::Open() {
  assert(managers_.achievements != nullptr);
  if (shown_ == nullptr) {
    shown_ = new AchievementsWindow(managers_.achievements, window_);
    shown_->setAttribute(Qt::WA_DeleteOnClose);
  }
  shown_->show();
  shown_->raise();
}

}  // namespace snapper
