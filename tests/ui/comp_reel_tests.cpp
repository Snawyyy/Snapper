#include <QTest>

#include "anim/reel_timeline.h"
#include "bench.h"
#include "ui/comp_page.h"

namespace snapper {

class CompReelTests final : public QObject {
  Q_OBJECT

 private slots:
  void ShotAtReelFindsTheShotUnderThePlayhead();
  void ThePageFollowsTheReelPlayhead();
};

void CompReelTests::ShotAtReelFindsTheShotUnderThePlayhead() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  QVERIFY(bench.shots.Add(-1).has_value());
  const ShotId second = bench.history.current().shots[1]->id;
  QVERIFY(bench.reel.AddShot(second, 0, Frame(10)).has_value());
  const Project& project = bench.history.current();
  QVERIFY(!ShotAtReel(project, Frame(5)).IsValid());
  const ReelShot at = ShotAtReel(project, Frame(14));
  QVERIFY(at.IsValid());
  QCOMPARE(at.shot, second);
  QCOMPARE(at.local, Frame(4));
}

void CompReelTests::ThePageFollowsTheReelPlayhead() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  QVERIFY(bench.shots.Add(-1).has_value());
  const ShotId second = bench.history.current().shots[1]->id;
  QVERIFY(bench.reel.AddShot(second, 0, Frame(10)).has_value());
  bench.playback.SetTimeline(Timeline::kReel);
  CompPage page(bench.All());
  page.resize(1200, 700);
  page.show();
  QTest::qWait(30);
  bench.playback.Seek(Frame(14));
  QCOMPARE(bench.selection.shot(), second);
  QVERIFY(bench.comp.Add(second, CompKind::kRimLight).has_value());
  QTest::qWait(30);
}

}  // namespace snapper

QTEST_MAIN(snapper::CompReelTests)
#include "comp_reel_tests.moc"
