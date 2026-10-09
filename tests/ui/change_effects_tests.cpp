#include <QSignalSpy>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/change_effects.h"

namespace snapper {
namespace {

const LayerId kBob(1);

KeyChanges Added(int frame) {
  assert(frame >= 0);
  assert(kBob.IsValid());
  KeyChanges changes;
  changes.added.push_back({kBob, Frame(frame)});
  return changes;
}

KeyChanges Removed(int frame) {
  assert(frame >= 0);
  assert(kBob.IsValid());
  KeyChanges changes;
  changes.removed.push_back({kBob, Frame(frame)});
  return changes;
}

}  // namespace

class ChangeEffectsTests final : public QObject {
  Q_OBJECT

 private slots:
  void CurvesStartBigAndEndAtRest();
  void PuffShrinksAndFadesToNothing();
  void AFreshKeyPopsWithARing();
  void ADeletedKeyPuffsThenGoes();
  void UndoRewindsWhatItBringsBack();
  void ASlideShowsOnlyOnUndoAndRedo();
  void OffShowsNothing();
  void CalmStillPlays();
  void ClipsPopPuffAndRewindToo();
  void ThingsAreToldApartByLaneItemAndFrame();
  void ASongsWorthOfKeysIsCapped();
};

void ChangeEffectsTests::CurvesStartBigAndEndAtRest() {
  QCOMPARE(PopScale(0.0), 1.4);
  QCOMPARE(PopScale(1.0), 1.0);
  QCOMPARE(RewindScale(0.0), 1.25);
  QCOMPARE(RewindScale(1.0), 1.0);
  QCOMPARE(RewindGlow(0.0), 1.0);
  QCOMPARE(RewindGlow(1.0), 0.0);
}

void ChangeEffectsTests::PuffShrinksAndFadesToNothing() {
  QCOMPARE(PuffScale(0.0), 1.0);
  QCOMPARE(PuffScale(1.0), 0.0);
  QCOMPARE(PuffOpacity(0.0), 1.0);
  QCOMPARE(PuffOpacity(1.0), 0.0);
  // The curve may overshoot; the puff never grows back.
  QCOMPARE(PuffScale(1.1), 0.0);
  QCOMPARE(PuffOpacity(1.1), 0.0);
}

void ChangeEffectsTests::AFreshKeyPopsWithARing() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  QSignalSpy changed(&effects, &ChangeEffects::Changed);
  effects.ShowKeys(Added(4), false);
  QVERIFY(effects.IsPlaying());
  QVERIFY(effects.ScaleOf(1, 0, Frame(4)) > 1.0);
  QVERIFY(effects.RingOf(1, 0, Frame(4)).has_value());
  QCOMPARE(effects.GlowOf(1, 0, Frame(4)), 0.0);
  // A key that did not change stays as it is.
  QCOMPARE(effects.ScaleOf(1, 0, Frame(5)), 1.0);
  QVERIFY(!effects.RingOf(1, 0, Frame(5)).has_value());
  QTRY_VERIFY(!effects.IsPlaying());
  QVERIFY(!changed.isEmpty());
  QCOMPARE(effects.ScaleOf(1, 0, Frame(4)), 1.0);
}

void ChangeEffectsTests::ADeletedKeyPuffsThenGoes() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  effects.ShowKeys(Removed(7), false);
  QCOMPARE(effects.Puffs().size(), size_t{1});
  QCOMPARE(effects.Puffs()[0].frame, Frame(7));
  QCOMPARE(effects.Puffs()[0].lane, 1);
  QTRY_VERIFY(effects.Puffs().empty());
  QVERIFY(!effects.IsPlaying());
}

void ChangeEffectsTests::UndoRewindsWhatItBringsBack() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  effects.ShowKeys(Added(4), true);
  QVERIFY(effects.ScaleOf(1, 0, Frame(4)) > 1.0);
  QVERIFY(effects.GlowOf(1, 0, Frame(4)) > 0.0);
  // Rewinding has no ring: that is for fresh keys.
  QVERIFY(!effects.RingOf(1, 0, Frame(4)).has_value());
  QTRY_VERIFY(!effects.IsPlaying());
}

void ChangeEffectsTests::ASlideShowsOnlyOnUndoAndRedo() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  KeyChanges moved;
  moved.moved.push_back({kBob, Frame(2), Frame(6)});
  effects.ShowKeys(moved, false);
  QVERIFY(!effects.IsPlaying());
  effects.ShowKeys(moved, true);
  QVERIFY(effects.GlowOf(1, 0, Frame(6)) > 0.0);
  QCOMPARE(effects.GlowOf(1, 0, Frame(2)), 0.0);
}

void ChangeEffectsTests::OffShowsNothing() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  effects.ShowKeys(Added(4), false);
  effects.ShowKeys(Removed(7), false);
  effects.ShowKeys(Added(4), true);
  QVERIFY(!effects.IsPlaying());
  QCOMPARE(effects.ScaleOf(1, 0, Frame(4)), 1.0);
  QVERIFY(effects.Puffs().empty());
}

void ChangeEffectsTests::CalmStillPlays() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kCalm);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  effects.ShowKeys(Added(4), false);
  QVERIFY(effects.IsPlaying());
  QTRY_VERIFY(!effects.IsPlaying());
}

void ChangeEffectsTests::ClipsPopPuffAndRewindToo() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  ClipChanges changes;
  changes.added.push_back({ClipId(3), 1, Frame(10), Frame(20)});
  changes.removed.push_back({ClipId(4), 0, Frame(40), Frame(8)});
  effects.ShowClips(changes, false);
  QVERIFY(effects.ScaleOf(1, 3, Frame(10)) > 1.0);
  QCOMPARE(effects.Puffs().size(), size_t{1});
  QCOMPARE(effects.Puffs()[0].length, Frame(8));
  QCOMPARE(effects.Puffs()[0].item, 4);
  QTRY_VERIFY(!effects.IsPlaying());
  effects.ShowClips(changes, true);
  QVERIFY(effects.GlowOf(1, 3, Frame(10)) > 0.0);
  QTRY_VERIFY(!effects.IsPlaying());
}

void ChangeEffectsTests::ThingsAreToldApartByLaneItemAndFrame() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  effects.ShowKeys(Added(4), false);
  QCOMPARE(effects.ScaleOf(2, 0, Frame(4)), 1.0);
  QCOMPARE(effects.ScaleOf(1, 9, Frame(4)), 1.0);
  QCOMPARE(effects.ScaleOf(1, 0, Frame(3)), 1.0);
  QVERIFY(effects.ScaleOf(1, 0, Frame(4)) > 1.0);
}

void ChangeEffectsTests::ASongsWorthOfKeysIsCapped() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  ChangeEffects effects(&bench.motion, &owner);
  KeyChanges many;
  for (int f = 0; f < 1000; ++f) {
    many.removed.push_back({kBob, Frame(f)});
  }
  effects.ShowKeys(many, false);
  QVERIFY(effects.Puffs().size() <= size_t{256});
  QVERIFY(!effects.Puffs().empty());
  QTRY_VERIFY(!effects.IsPlaying());
}

}  // namespace snapper

QTEST_MAIN(snapper::ChangeEffectsTests)
#include "change_effects_tests.moc"
