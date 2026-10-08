#include <QComboBox>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include <cassert>
#include <set>

#include "anim/reel_timeline.h"
#include "bench.h"
#include "ui/reel_timeline.h"
#include "ui/transition_picker.h"

namespace snapper {
namespace {

// The bench with one 48-frame shot on the reel's tracks 0 and 1, the
// playhead on the reel.
struct Scene final {
  Scene() : timeline(bench.All()) {
    QVERIFY(bench.shots.Add(-1).has_value());
    bench.playback.SetTimeline(Timeline::kReel);
    low = *bench.reel.AddShot(ShotId(1), 0, Frame(0));
    high = *bench.reel.AddShot(ShotId(1), 1, Frame(100));
    timeline.resize(1000, 200);
  }
  QPoint At(Frame frame, int track) const {
    assert(track >= 0);
    return QPoint(static_cast<int>(timeline.XOf(frame)),
                  static_cast<int>(timeline.TrackTop(track)) + 10);
  }
  Bench bench;
  ReelTimeline timeline;
  ClipId low;
  ClipId high;
};

}  // namespace

class ReelTimelineTests final : public QObject {
  Q_OBJECT

 private slots:
  void BoxPicksAndEmptyClickClears();
  void EscapeCancelsADrag();
  void KeysCopyPasteAndDuplicate();
  void ShiftDeleteClosesTheGap();
  void KeysThatCantActSayWhy();
  void JointsAreWhereTouchingClipsMeet();
  void PickerOffersKindAndLength();
};

void ReelTimelineTests::BoxPicksAndEmptyClickClears() {
  Scene s;
  // From empty space on track 1 down over both clips.
  const QPoint from = s.At(Frame(60), 1);
  const QPoint to = s.At(Frame(110), 0);
  QTest::mousePress(&s.timeline, Qt::LeftButton, {}, from);
  QTest::mouseMove(&s.timeline, to);
  QTest::mouseRelease(&s.timeline, Qt::LeftButton, {}, to);
  QCOMPARE(s.bench.selection.clips(), (std::set<ClipId>{s.high}));
  const QPoint left = s.At(Frame(60), 0);
  QTest::mousePress(&s.timeline, Qt::LeftButton, Qt::ShiftModifier, left);
  QTest::mouseMove(&s.timeline, s.At(Frame(20), 0));
  QTest::mouseRelease(&s.timeline, Qt::LeftButton, Qt::ShiftModifier,
                      s.At(Frame(20), 0));
  QCOMPARE(s.bench.selection.clips(), (std::set<ClipId>{s.low, s.high}));
  QTest::mouseClick(&s.timeline, Qt::LeftButton, {}, s.At(Frame(300), 0));
  QVERIFY(s.bench.selection.clips().empty());
}

void ReelTimelineTests::EscapeCancelsADrag() {
  Scene s;
  const QPoint from = s.At(Frame(24), 0);
  QTest::mousePress(&s.timeline, Qt::LeftButton, {}, from);
  QTest::mouseMove(&s.timeline, s.At(Frame(36), 0));
  QTest::mouseMove(&s.timeline, s.At(Frame(48), 0));
  QCOMPARE(ClipOf(s.bench.history.current().reel, s.low)->start, Frame(24));
  QTest::keyClick(&s.timeline, Qt::Key_Escape);
  QTest::mouseRelease(&s.timeline, Qt::LeftButton, {}, s.At(Frame(48), 0));
  QCOMPARE(ClipOf(s.bench.history.current().reel, s.low)->start, Frame(0));
  QCOMPARE(s.bench.history.UndoLabel(), QString("Add shot to video"));
  // The pick stays; a second Escape clears it.
  QCOMPARE(s.bench.selection.clips(), (std::set<ClipId>{s.low}));
  QTest::keyClick(&s.timeline, Qt::Key_Escape);
  QVERIFY(s.bench.selection.clips().empty());
}

void ReelTimelineTests::KeysCopyPasteAndDuplicate() {
  Scene s;
  s.bench.selection.PickClips({s.low}, PickMode::kReplace);
  QTest::keyClick(&s.timeline, Qt::Key_C, Qt::ControlModifier);
  s.bench.playback.Seek(Frame(120));
  QTest::keyClick(&s.timeline, Qt::Key_V, Qt::ControlModifier);
  const auto& bottom = s.bench.history.current().reel.tracks[0].clips;
  QCOMPARE(bottom.size(), size_t{2});
  QCOMPARE(bottom[1].start, Frame(120));
  // The pasted clip is picked, so Ctrl+D lays another after it.
  QCOMPARE(s.bench.selection.clips(), (std::set<ClipId>{bottom[1].id}));
  QTest::keyClick(&s.timeline, Qt::Key_D, Qt::ControlModifier);
  QCOMPARE(s.bench.history.current().reel.tracks[0].clips.size(),
           size_t{3});
  QCOMPARE(s.bench.history.current().reel.tracks[0].clips[2].start,
           Frame(168));
  QCOMPARE(s.bench.history.UndoLabel(), QString("Duplicate clip"));
  QTest::keyClick(&s.timeline, Qt::Key_X, Qt::ControlModifier);
  QCOMPARE(s.bench.history.UndoLabel(), QString("Cut clip"));
}

void ReelTimelineTests::ShiftDeleteClosesTheGap() {
  Scene s;
  const ClipId after = *s.bench.reel.AddShot(ShotId(1), 0, Frame(60));
  s.bench.selection.PickClips({s.low}, PickMode::kReplace);
  QTest::keyClick(&s.timeline, Qt::Key_Delete, Qt::ShiftModifier);
  QCOMPARE(ClipOf(s.bench.history.current().reel, after)->start, Frame(12));
  QCOMPARE(s.bench.history.UndoLabel(), QString("Ripple remove clip"));
}

void ReelTimelineTests::KeysThatCantActSayWhy() {
  Scene s;
  QSignalSpy problems(&s.timeline, &ReelTimeline::Problem);
  QTest::keyClick(&s.timeline, Qt::Key_V, Qt::ControlModifier);
  QCOMPARE(problems.size(), 1);
  QCOMPARE(problems[0][0].toString(), QString("Copy a clip first."));
  QTest::keyClick(&s.timeline, Qt::Key_Delete);
  QCOMPARE(problems.size(), 2);
  QCOMPARE(s.bench.history.current().reel.tracks[0].clips.size(),
           size_t{1});
}

void ReelTimelineTests::JointsAreWhereTouchingClipsMeet() {
  Scene s;
  const ClipId next = *s.bench.reel.AddShot(ShotId(1), 0, Frame(48));
  QVERIFY(next.IsValid());
  const QPoint cut = s.At(Frame(48), 0);
  QCOMPARE(s.timeline.JointAt(cut), s.low);
  QCOMPARE(s.timeline.JointAt(cut + QPoint(4, 0)), s.low);
  QVERIFY(!s.timeline.JointAt(cut + QPoint(30, 0)).IsValid());
  // The lone clip's end on track 1 meets nothing.
  QVERIFY(!s.timeline.JointAt(s.At(Frame(148), 1)).IsValid());
}

void ReelTimelineTests::PickerOffersKindAndLength() {
  TransitionPicker picker(Transition(), Frame(9));
  auto* kind = picker.findChild<QComboBox*>("transition_kind");
  auto* length = picker.findChild<QSpinBox*>("transition_length");
  QCOMPARE(kind->count(), kTransitionKindCount);
  QVERIFY(!length->isEnabled());
  QCOMPARE(picker.transition(), Transition());
  kind->setCurrentIndex(static_cast<int>(TransitionKind::kFlash));
  QVERIFY(length->isEnabled());
  QCOMPARE(length->maximum(), 9);
  QCOMPARE(picker.transition(),
           (Transition{TransitionKind::kFlash, Frame(4)}));
  TransitionPicker tight(Transition{TransitionKind::kCrossfade, Frame(1)},
                         Frame(1));
  QCOMPARE(tight.findChild<QSpinBox*>("transition_length")->maximum(), 1);
}

}  // namespace snapper

QTEST_MAIN(snapper::ReelTimelineTests)
#include "reel_timeline_tests.moc"
