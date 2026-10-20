#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <vector>

#include "edit/achievement_manager.h"
#include "edit/character_manager.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

int IndexOf(const char* id) {
  for (int i = 0; i < kAchievementCount; ++i) {
    if (QString::fromLatin1(kAchievements[i].id) == QLatin1String(id)) {
      return i;
    }
  }
  return -1;
}

}  // namespace

class AchievementTests final : public QObject {
  Q_OBJECT

 private slots:
  void DrawingEarnsAndUndoNeverTakesBack();
  void DaysInARowMakeAStreak();
  void TheCaseOutlivesTheRun();
};

void AchievementTests::DrawingEarnsAndUndoNeverTakesBack() {
  QTemporaryDir library;
  HistoryManager history{Project()};
  CharacterManager characters(&history, library.path());
  AchievementManager trophies(&history, nullptr, QString());
  QSignalSpy earned(&trophies, &AchievementManager::Earned);
  QVERIFY(characters.Create("Bo").has_value());
  QVERIFY(trophies.IsEarned(IndexOf("its_alive")));
  const Character* bo = FindCharacter(history.current(), "Bo");
  const PartId drawing = bo->parts[1].id;
  const std::vector<QPointF> line = {{0, 0}, {40, 0}};
  QVERIFY(characters.AddCurve("Bo", drawing, line, false, 0.0).has_value());
  QVERIFY(trophies.IsEarned(IndexOf("timber")));
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kStrokes), 1);
  // Undo takes the stroke away but not the trophy or the count; redo
  // does not count it twice.
  history.Undo();
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kUndos), 1);
  history.Redo();
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kStrokes), 1);
  QVERIFY(trophies.IsEarned(IndexOf("timber")));
  QVERIFY(earned.count() >= 2);
  // What the project cannot show comes from outside.
  trophies.Count(AchievementStat::kLightTable);
  QVERIFY(trophies.IsEarned(IndexOf("seeing_ghosts")));
  for (int tool = 0; tool < 14; ++tool) {
    trophies.UseTool(tool);
  }
  QVERIFY(trophies.IsEarned(IndexOf("tool_time")));
}

void AchievementTests::DaysInARowMakeAStreak() {
  QTemporaryDir library;
  HistoryManager history{Project()};
  CharacterManager characters(&history, library.path());
  QDateTime now(QDate(2026, 10, 1), QTime(23, 0));
  AchievementManager trophies(&history, nullptr, QString(),
                              [&now] { return now; });
  for (int day = 0; day < 3; ++day) {
    QVERIFY(characters.Create(QString("C%1").arg(day)).has_value());
    now = now.addDays(1);
  }
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kStreak), 3);
  QVERIFY(trophies.IsEarned(IndexOf("not_a_phase")));
  QVERIFY(!trophies.IsEarned(IndexOf("night_owl")));
  now = QDateTime(now.date().addDays(5), QTime(1, 30));
  QVERIFY(characters.Create("Late").has_value());
  QVERIFY(trophies.IsEarned(IndexOf("night_owl")));
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kStreak), 3);
}

void AchievementTests::TheCaseOutlivesTheRun() {
  QTemporaryDir folder;
  const QString settings = folder.filePath("settings.ini");
  {
    HistoryManager history{Project()};
    AchievementManager trophies(&history, nullptr, settings);
    trophies.Count(AchievementStat::kExports);
    QVERIFY(trophies.IsEarned(IndexOf("fresh_press")));
  }
  HistoryManager history{Project()};
  AchievementManager trophies(&history, nullptr, settings);
  QVERIFY(trophies.IsEarned(IndexOf("fresh_press")));
  QCOMPARE(StatOf(trophies.stats(), AchievementStat::kExports), 1);
  QCOMPARE(trophies.EarnedCount(), 1);
}

}  // namespace snapper

QTEST_MAIN(snapper::AchievementTests)
#include "achievement_tests.moc"
