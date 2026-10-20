#include "edit/achievement_manager.h"

#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <bit>
#include <cassert>

#include "edit/export_manager.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

constexpr char kGroup[] = "achievements";
// Work before this hour counts as past midnight.
constexpr int kNightEnds = 5;

}  // namespace

AchievementManager::AchievementManager(HistoryManager* history,
                                       ExportManager* exporter,
                                       const QString& settings_path,
                                       Clock clock)
    : history_(history),
      settings_path_(settings_path),
      clock_(std::move(clock)) {
  assert(history_ != nullptr);
  assert(clock_ != nullptr);
  Load();
  measured_ = Measure(history_->current());
  last_revision_ = newest_revision_ = history_->revision();
  connect(history_, &HistoryManager::Changed, this,
          &AchievementManager::Watch);
  if (exporter != nullptr) {
    connect(exporter, &ExportManager::Finished, this,
            [this] { Count(AchievementStat::kExports); });
  }
}

bool AchievementManager::IsEarned(int index) const {
  assert(index >= 0 && index < kAchievementCount);
  return earned_[static_cast<size_t>(index)];
}

int AchievementManager::EarnedCount() const {
  return static_cast<int>(std::count(earned_.begin(), earned_.end(), true));
}

void AchievementManager::Count(AchievementStat stat, int by) {
  assert(by >= 0);
  assert(static_cast<int>(stat) < kAchievementStatCount);
  stats_[static_cast<size_t>(stat)] += by;
  Check();
}

void AchievementManager::UseTool(int tool) {
  assert(tool >= 0 && tool < 32);
  const quint32 bit = 1u << static_cast<unsigned>(tool);
  if ((tools_ & bit) != 0) {
    return;
  }
  tools_ |= bit;
  int& used = stats_[static_cast<size_t>(AchievementStat::kToolsUsed)];
  used = std::popcount(tools_);
  Check();
}

void AchievementManager::Watch() {
  assert(history_ != nullptr);
  const int revision = history_->revision();
  if (revision == last_revision_) {
    return;
  }
  const AchievementStats now = Measure(history_->current());
  // A step never seen before, made by hand (a fresh project or an opened
  // file has nothing to undo), counts as work; going back is an undo.
  const bool is_new_step =
      revision > newest_revision_ && history_->CanUndo();
  const bool is_undo = revision < last_revision_;
  for (int i = 0; i < kAchievementStatCount; ++i) {
    const auto stat = static_cast<AchievementStat>(i);
    const size_t at = static_cast<size_t>(i);
    if (IsTallied(stat)) {
      stats_[at] += is_new_step ? std::max(0, now[at] - measured_[at]) : 0;
    } else {
      stats_[at] = std::max(stats_[at], now[at]);
    }
  }
  if (is_undo) {
    ++stats_[static_cast<size_t>(AchievementStat::kUndos)];
  }
  if (is_new_step) {
    MarkDay();
  }
  measured_ = now;
  last_revision_ = revision;
  newest_revision_ = std::max(newest_revision_, revision);
  Check();
}

void AchievementManager::MarkDay() {
  const QDateTime now = clock_();
  const QDate today = now.date();
  const bool is_next_day = last_day_.isValid() && last_day_.addDays(1) == today;
  if (is_next_day) {
    ++run_days_;
  } else if (today != last_day_) {
    run_days_ = 1;
  }
  last_day_ = today;
  int& streak = stats_[static_cast<size_t>(AchievementStat::kStreak)];
  streak = std::max(streak, run_days_);
  const bool is_night = now.time().hour() < kNightEnds;
  if (is_night) {
    stats_[static_cast<size_t>(AchievementStat::kNightOwl)] = 1;
  }
}

void AchievementManager::Check() {
  for (int i = 0; i < kAchievementCount; ++i) {
    const size_t at = static_cast<size_t>(i);
    const bool is_new = !earned_[at] && snapper::IsEarned(kAchievements[at],
                                                          stats_);
    if (is_new) {
      earned_[at] = true;
      emit Earned(i);
    }
  }
  Save();
  emit Changed();
}

void AchievementManager::Load() {
  if (settings_path_.isEmpty()) {
    return;
  }
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.beginGroup(kGroup);
  for (int i = 0; i < kAchievementStatCount; ++i) {
    stats_[static_cast<size_t>(i)] =
        settings.value(QStringLiteral("stat%1").arg(i), 0).toInt();
  }
  const QStringList earned = settings.value("earned").toStringList();
  for (int i = 0; i < kAchievementCount; ++i) {
    earned_[static_cast<size_t>(i)] =
        earned.contains(QString::fromLatin1(kAchievements[i].id));
  }
  last_day_ = settings.value("last_day").toDate();
  run_days_ = settings.value("run_days", 0).toInt();
  tools_ = settings.value("tools", 0u).toUInt();
}

void AchievementManager::Save() const {
  if (settings_path_.isEmpty()) {
    return;
  }
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.beginGroup(kGroup);
  for (int i = 0; i < kAchievementStatCount; ++i) {
    settings.setValue(QStringLiteral("stat%1").arg(i),
                      stats_[static_cast<size_t>(i)]);
  }
  QStringList earned;
  for (int i = 0; i < kAchievementCount; ++i) {
    if (earned_[static_cast<size_t>(i)]) {
      earned.append(QString::fromLatin1(kAchievements[i].id));
    }
  }
  settings.setValue("earned", earned);
  settings.setValue("last_day", last_day_);
  settings.setValue("run_days", run_days_);
  settings.setValue("tools", tools_);
}

}  // namespace snapper
