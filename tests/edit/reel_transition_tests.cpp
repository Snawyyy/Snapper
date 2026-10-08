#include <QTest>

#include <cassert>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "edit/shot_manager.h"

namespace snapper {
namespace {

// Two 48-frame clips of one shot meeting at frame 48 on track 0, and a
// lone one on track 1.
struct Fixture final {
  Fixture() : history(Project()), shots(&history), reel(&history) {
    const ShotId shot = *shots.Add(-1);
    left = *reel.AddShot(shot, 0, Frame(0));
    right = *reel.AddShot(shot, 0, Frame(48));
    lone = *reel.AddShot(shot, 1, Frame(0));
  }
  const Clip& Of(ClipId id) const {
    const Clip* clip = ClipOf(history.current().reel, id);
    assert(clip != nullptr);
    return *clip;
  }
  HistoryManager history;
  ShotManager shots;
  ReelManager reel;
  ClipId left;
  ClipId right;
  ClipId lone;
};

}  // namespace

class ReelTransitionTests final : public QObject {
  Q_OBJECT

 private slots:
  void TransitionsSitOnTouchingCuts();
  void LengthIsBoundByBothClips();
  void SplittingKeepsTheTransitionAtTheEnd();
  void NamesReadPlainly();
};

void ReelTransitionTests::TransitionsSitOnTouchingCuts() {
  Fixture f;
  QVERIFY(f.reel.WhyNoTransition(f.left).isEmpty());
  QVERIFY(!f.reel.WhyNoTransition(f.right).isEmpty());
  QVERIFY(!f.reel.WhyNoTransition(f.lone).isEmpty());
  QVERIFY(!f.reel.WhyNoTransition(ClipId(99)).isEmpty());
  const Transition fade{TransitionKind::kCrossfade, Frame(6)};
  QVERIFY(f.reel.SetTransition(f.left, fade).has_value());
  QCOMPARE(f.Of(f.left).out, fade);
  QCOMPARE(f.history.UndoLabel(), QString("Change transition"));
  QVERIFY(!f.reel.SetTransition(f.lone, fade).has_value());
  // Back to a cut forgets the length.
  QVERIFY(f.reel
              .SetTransition(f.left, {TransitionKind::kCut, Frame(6)})
              .has_value());
  QCOMPARE(f.Of(f.left).out, Transition());
  f.history.Undo();
  QCOMPARE(f.Of(f.left).out, fade);
}

void ReelTransitionTests::LengthIsBoundByBothClips() {
  Fixture f;
  QVERIFY(f.reel.TrimEnd(f.right, Frame(58)).has_value());
  const Transition flash{TransitionKind::kFlash, Frame(10)};
  const auto refused = f.reel.SetTransition(f.left, flash);
  QVERIFY(!refused.has_value());
  QVERIFY(refused.error().message.contains("1 to 9"));
  QVERIFY(f.reel
              .SetTransition(f.left, {TransitionKind::kFlash, Frame(9)})
              .has_value());
  QVERIFY(!f.reel
               .SetTransition(f.left, {TransitionKind::kFlash, Frame(0)})
               .has_value());
  // Both clips one frame long: no room for any mix.
  QVERIFY(f.reel.TrimStart(f.left, Frame(47)).has_value());
  QVERIFY(f.reel.TrimEnd(f.right, Frame(49)).has_value());
  QVERIFY(f.reel.WhyNoTransition(f.left).contains("two frames"));
}

void ReelTransitionTests::SplittingKeepsTheTransitionAtTheEnd() {
  Fixture f;
  const Transition swipe{TransitionKind::kSwipeUp, Frame(4)};
  QVERIFY(f.reel.SetTransition(f.left, swipe).has_value());
  const auto half = f.reel.Split(f.left, Frame(20));
  QVERIFY(half.has_value());
  QCOMPARE(f.Of(f.left).out, Transition());
  QCOMPARE(f.Of(*half).out, swipe);
  // Copies carry theirs along.
  QVERIFY(f.reel.Copy({*half}).has_value());
  const auto pasted = f.reel.Paste(Frame(300));
  QVERIFY(pasted.has_value());
  QCOMPARE(f.Of(pasted->front()).out, swipe);
}

void ReelTransitionTests::NamesReadPlainly() {
  QCOMPARE(TransitionName(TransitionKind::kCut), QString("Cut"));
  QCOMPARE(TransitionName(TransitionKind::kCrossfade), QString("Crossfade"));
  QCOMPARE(TransitionName(TransitionKind::kSwipeLeft), QString("Swipe left"));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::ReelTransitionTests)
#include "reel_transition_tests.moc"
