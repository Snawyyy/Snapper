#ifndef SNAPPER_EDIT_ACHIEVEMENT_MANAGER_H_
#define SNAPPER_EDIT_ACHIEVEMENT_MANAGER_H_

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <array>
#include <functional>

#include "edit/achievements.h"

namespace snapper {

class ExportManager;
class HistoryManager;

// Keeps the trophy case: the stats behind every achievement and which
// are earned, kept in the app's settings so they outlive any project.
// It reads the project after every change (the most bones in one
// character, strokes drawn since), counts undos and finished exports
// itself, and takes the rest from whoever sees them (Count). Earned
// trophies stay earned. Never touches the project.
class AchievementManager final : public QObject {
  Q_OBJECT

 public:
  using Clock = std::function<QDateTime()>;

  // exporter may be null (no Fresh Off the Press then); settings_path
  // empty keeps nothing between runs.
  AchievementManager(HistoryManager* history, ExportManager* exporter,
                     const QString& settings_path,
                     Clock clock = [] {
                       return QDateTime::currentDateTime();
                     });

  const AchievementStats& stats() const { return stats_; }
  bool IsEarned(int index) const;
  int EarnedCount() const;
  // Something the project cannot show happened (the light table lit, a
  // note dismissed): stat goes up by by.
  void Count(AchievementStat stat, int by = 1);
  // A Character tab tool was used, by its index (0 to 31).
  void UseTool(int tool);

 signals:
  // The achievement at index in kAchievements was just earned.
  void Earned(int index);
  void Changed();

 private:
  void Watch();
  // Today was worked: the streak goes on, starts over or stays.
  void MarkDay();
  void Check();
  void Load();
  void Save() const;

  HistoryManager* history_;
  QString settings_path_;
  Clock clock_;
  AchievementStats stats_{};
  // What the project measured last, to tell how much a step raised.
  AchievementStats measured_{};
  std::array<bool, kAchievementCount> earned_{};
  int last_revision_ = 0;
  int newest_revision_ = 0;
  QDate last_day_;
  int run_days_ = 0;
  quint32 tools_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_ACHIEVEMENT_MANAGER_H_
