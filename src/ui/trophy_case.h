#ifndef SNAPPER_UI_TROPHY_CASE_H_
#define SNAPPER_UI_TROPHY_CASE_H_

#include <QAction>
#include <QObject>
#include <QPointer>

#include "ui/managers.h"

namespace snapper {

class AchievementsWindow;
class CharacterSession;
class StageView;

// Ties the achievements to the window: tells AchievementManager what
// only the UI sees (the light table lit, a Character tab tool picked, a
// note taken away), shows a gold note when a trophy is earned, and opens
// the trophy case from its action. Does nothing without an
// AchievementManager.
class TrophyCase final : public QObject {
  Q_OBJECT

 public:
  TrophyCase(const Managers& managers, StageView* stage,
             CharacterSession* session, QWidget* window);

  // View > Achievements...
  QAction* action() { return &open_; }

 private:
  void Open();

  Managers managers_;
  QWidget* window_;
  QAction open_;
  QPointer<AchievementsWindow> shown_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TROPHY_CASE_H_
