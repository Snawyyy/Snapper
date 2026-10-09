#include <QSignalSpy>
#include <QTest>

#include "bench.h"
#include "ui/playhead_settle.h"

namespace snapper {

class PlayheadSettleTests final : public QObject {
  Q_OBJECT

 private slots:
  void StartsAtRest();
  void SlidesFromTheOldFrameThenRests();
  void OvershootsInFullOnly();
  void OffLandsAtOnce();
  void PlayingLandsAtOnce();
  void ALongJumpLandsAtOnce();
  void AMoveInMidSettleCarriesOnFromWhereItIsDrawn();
};

void PlayheadSettleTests::StartsAtRest() {
  Bench bench;
  PlayheadSettle settle(&bench.motion, Frame(5), this);
  QCOMPARE(settle.offset(), 0.0);
  settle.MoveTo(Frame(5), false);
  QCOMPARE(settle.offset(), 0.0);
}

void PlayheadSettleTests::SlidesFromTheOldFrameThenRests() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  PlayheadSettle settle(&bench.motion, Frame(5), this);
  QSignalSpy changed(&settle, &PlayheadSettle::Changed);
  settle.MoveTo(Frame(9), false);
  // It is drawn at the old frame to begin with: 4 frames behind.
  QVERIFY(settle.offset() <= 0.0);
  QTRY_COMPARE(settle.offset(), 0.0);
  QVERIFY(!changed.isEmpty());
}

void PlayheadSettleTests::OvershootsInFullOnly() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  PlayheadSettle full(&bench.motion, Frame(0), this);
  double most_ahead = 0.0;
  connect(&full, &PlayheadSettle::Changed, this, [&] {
    most_ahead = std::max(most_ahead, full.offset());
  });
  full.MoveTo(Frame(10), false);
  QTRY_COMPARE(full.offset(), 0.0);
  QVERIFY(most_ahead > 0.0);
  bench.motion.SetMode(MotionMode::kCalm);
  PlayheadSettle calm(&bench.motion, Frame(0), this);
  double calm_ahead = 0.0;
  connect(&calm, &PlayheadSettle::Changed, this, [&] {
    calm_ahead = std::max(calm_ahead, calm.offset());
  });
  calm.MoveTo(Frame(10), false);
  QTRY_COMPARE(calm.offset(), 0.0);
  QCOMPARE(calm_ahead, 0.0);
}

void PlayheadSettleTests::OffLandsAtOnce() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  PlayheadSettle settle(&bench.motion, Frame(5), this);
  settle.MoveTo(Frame(9), false);
  QCOMPARE(settle.offset(), 0.0);
  QVERIFY(settle.parent() == this);
}

void PlayheadSettleTests::PlayingLandsAtOnce() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  PlayheadSettle settle(&bench.motion, Frame(5), this);
  settle.MoveTo(Frame(6), true);
  QCOMPARE(settle.offset(), 0.0);
  QVERIFY(settle.parent() == this);
}

void PlayheadSettleTests::ALongJumpLandsAtOnce() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  PlayheadSettle settle(&bench.motion, Frame(0), this);
  settle.MoveTo(Frame(kMaxSettleFrames + 1), false);
  QCOMPARE(settle.offset(), 0.0);
  settle.MoveTo(Frame(kMaxSettleFrames + 1 + kMaxSettleFrames), false);
  QVERIFY(settle.offset() <= 0.0);
}

void PlayheadSettleTests::AMoveInMidSettleCarriesOnFromWhereItIsDrawn() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  PlayheadSettle settle(&bench.motion, Frame(0), this);
  settle.MoveTo(Frame(10), false);
  settle.MoveTo(Frame(12), false);
  // Still drawn behind the new frame, not snapped to it.
  QVERIFY(settle.offset() < 0.0);
  QTRY_COMPARE(settle.offset(), 0.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::PlayheadSettleTests)
#include "playhead_settle_tests.moc"
