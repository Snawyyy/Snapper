#include "ui/achievements_window.h"

#include <QScrollArea>

#include <algorithm>
#include <cassert>
#include <memory>

#include "edit/achievement_manager.h"
#include "ui/achievement_icons.h"
#include "ui/panel_kit.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kBadgeSide = 48;
constexpr int kColumns = 4;

const char* GroupName(AchievementGroup group) {
  switch (group) {
    case AchievementGroup::kRigger:
      return "Rigger";
    case AchievementGroup::kDirector:
      return "Director";
    case AchievementGroup::kGrinder:
      return "Grinder";
    case AchievementGroup::kArtist:
      break;
  }
  return "Artist";
}

}  // namespace

AchievementsWindow::AchievementsWindow(AchievementManager* achievements,
                                       QWidget* parent)
    : QDialog(parent), achievements_(achievements), layout_(this) {
  assert(achievements_ != nullptr);
  setWindowTitle(tr("Achievements"));
  resize(560, 640);
  Build();
  connect(achievements_, &AchievementManager::Changed, this,
          &AchievementsWindow::Refresh);
  Refresh();
}

void AchievementsWindow::Build() {
  assert(badges_.empty());
  stats_.setWordWrap(true);
  layout_.addWidget(&summary_);
  layout_.addWidget(&stats_);
  auto* shelves = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(shelves);
  column->setContentsMargins(0, 0, 0, 0);
  for (int group = 0; group < kAchievementGroupCount; ++group) {
    auto* body = MakeOwned<QWidget>(shelves);
    auto* grid = MakeOwned<QGridLayout>(body);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(theme::kCellGap);
    int at = 0;
    for (int i = 0; i < kAchievementCount; ++i) {
      if (static_cast<int>(kAchievements[i].group) != group) {
        continue;
      }
      auto* badge = MakeOwned<QLabel>(body);
      auto* caption = MakeOwned<QLabel>(body);
      caption->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
      caption->setWordWrap(true);
      badge->setAlignment(Qt::AlignHCenter);
      const QString tip =
          QStringLiteral("%1\n%2\n\"%3\"")
              .arg(tr(kAchievements[i].name), tr(kAchievements[i].how),
                   tr(kAchievements[i].joke));
      badge->setToolTip(tip);
      caption->setToolTip(tip);
      grid->addWidget(badge, 2 * (at / kColumns), at % kColumns);
      grid->addWidget(caption, 2 * (at / kColumns) + 1, at % kColumns);
      badges_.resize(static_cast<size_t>(kAchievementCount), nullptr);
      captions_.resize(static_cast<size_t>(kAchievementCount), nullptr);
      badges_[static_cast<size_t>(i)] = badge;
      captions_[static_cast<size_t>(i)] = caption;
      ++at;
    }
    column->addWidget(PlatedSection(
        tr(GroupName(static_cast<AchievementGroup>(group))), body, shelves));
  }
  column->addStretch(1);
  auto* scroll = MakeOwned<QScrollArea>(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(shelves);
  layout_.addWidget(scroll, 1);
}

void AchievementsWindow::Refresh() {
  assert(achievements_ != nullptr);
  assert(badges_.size() == static_cast<size_t>(kAchievementCount));
  const AchievementStats& stats = achievements_->stats();
  summary_.setText(tr("<b>%1 of %2 achievements</b>")
                       .arg(achievements_->EarnedCount())
                       .arg(kAchievementCount));
  const auto stat = [&stats](AchievementStat which) {
    return StatOf(stats, which);
  };
  stats_.setText(
      tr("Strokes drawn %1   Keys set %2   Shapes filled %3\n"
         "Videos burned %4   Undos %5   Best streak %6 days")
          .arg(stat(AchievementStat::kStrokes))
          .arg(stat(AchievementStat::kKeys))
          .arg(stat(AchievementStat::kFills))
          .arg(stat(AchievementStat::kExports))
          .arg(stat(AchievementStat::kUndos))
          .arg(stat(AchievementStat::kStreak)));
  for (int i = 0; i < kAchievementCount; ++i) {
    const size_t at = static_cast<size_t>(i);
    const bool is_earned = achievements_->IsEarned(i);
    badges_[at]->setPixmap(AchievementIcon(i, is_earned, kBadgeSide));
    const Achievement& trophy = kAchievements[i];
    const int now = std::min(StatOf(stats, trophy.stat), trophy.goal);
    // An earned trophy shows its name; one still to come shows how far
    // along it is, so the next goal is always in sight.
    captions_[at]->setText(is_earned ? tr(trophy.name)
                                     : QStringLiteral("%1\n%2 / %3")
                                           .arg(tr(trophy.name))
                                           .arg(now)
                                           .arg(trophy.goal));
    QPalette ink = captions_[at]->palette();
    ink.setColor(QPalette::WindowText,
                 is_earned ? theme::kHandle : theme::kTextOff);
    captions_[at]->setPalette(ink);
  }
}

}  // namespace snapper
