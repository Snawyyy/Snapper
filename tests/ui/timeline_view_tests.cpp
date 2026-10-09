#include <QTest>

#include <cassert>
#include <QToolButton>

#include "bench.h"
#include "edit/timeline_rows.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

const ShotId kShot(1);

// One doll layer with a key at frame 2, in a 48-frame shot.
void Stage(Bench* bench) {
  assert(bench != nullptr);
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}};
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Bob";
  layer.content = DollLayer{"Bob", {}, false};
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  QVERIFY(bench->pose.KeyInPlace(kShot, LayerId(1), Frame(2)).has_value());
}

// The middle of frame's cell on the doll's row.
QPoint Cell(const TimelineView& view, int frame) {
  assert(frame >= 0);
  assert(view.XOf(Frame(frame)) >= 0.0);
  return QPoint(static_cast<int>(view.XOf(Frame(frame)) +
                                 kDefaultFrameWidth / 2.0),
                kRulerHeight + kWaveHeight + kRowHeight / 2);
}

std::vector<Frame> DollKeys(const Bench& bench) {
  assert(!bench.history.current().shots.empty());
  assert(!TimelineRows(bench.history.current(), kShot).empty());
  return TimelineRows(bench.history.current(), kShot)[0].keys;
}

}  // namespace

class TimelineViewTests final : public QObject {
  Q_OBJECT

 private slots:
  void FramesMapToPixels();
  void ClickPicksAKeyAndDragSlidesIt();
  void DoubleClickKeysThePose();
  void DeleteRemovesPickedKeys();
  void RulerClickSeeks();
  void BoxShiftAndCtrlPickKeys();
  void KeysGrabFromTwelvePixelsAndWaitFourToSlide();
  void StepModeToggles();
  void KeyDragLeavesAGhostOfTheKey();
  void KeyDragSaysHowManyFrames();
  void EscapeCancelsTheKeyDrag();
  void ANewKeyPopsAndADeletedOnePuffs();
  void UndoRewindsAndTheDataIsAlreadyBack();
  void ASlidKeyPopsNothingAndOffShowsNothing();
  void UndoOfAMoveSlidesTheKeyBack();
};

void TimelineViewTests::FramesMapToPixels() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  QVERIFY(!view.FrameAt(kNameWidth - 1).has_value());
  QCOMPARE(*view.FrameAt(view.XOf(Frame(7)) + 1), Frame(7));
}

void TimelineViewTests::ClickPicksAKeyAndDragSlidesIt() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QCOMPARE(bench.selection.keys().size(), size_t{2});
  QCOMPARE(bench.selection.layer(), LayerId(1));
  QTest::mouseMove(&view, Cell(view, 6));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 6));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(6)}));
  QCOMPARE(bench.history.UndoLabel(), QString("Move keys"));
  QVERIFY(std::all_of(bench.selection.keys().begin(),
                      bench.selection.keys().end(),
                      [](const KeyRef& key) { return key.frame == Frame(6); }));
}

void TimelineViewTests::DoubleClickKeysThePose() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QVERIFY(view.isEnabled());
  QTest::mouseDClick(&view, Qt::LeftButton, {}, Cell(view, 10));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2), Frame(10)}));
}

void TimelineViewTests::DeleteRemovesPickedKeys() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QVERIFY(view.isEnabled());
  QTest::mouseClick(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::keyClick(&view, Qt::Key_Delete);
  QVERIFY(DollKeys(bench).empty());
}

void TimelineViewTests::RulerClickSeeks() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QVERIFY(view.isEnabled());
  const QPoint ruler(Cell(view, 9).x(), kRulerHeight / 2);
  QTest::mouseClick(&view, Qt::LeftButton, {}, ruler);
  QCOMPARE(bench.playback.frame(), Frame(9));
}

void TimelineViewTests::BoxShiftAndCtrlPickKeys() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.pose.KeyInPlace(kShot, LayerId(1), Frame(10)).has_value());
  TimelineView view(bench.All());
  view.resize(800, 200);
  // Frame 1 is within reach of the key at 2 and would grab it.
  const QPoint from = Cell(view, 0) + QPoint(0, -5);
  const QPoint to = Cell(view, 12) + QPoint(0, 5);
  QTest::mousePress(&view, Qt::LeftButton, {}, from);
  QTest::mouseMove(&view, to);
  QTest::mouseRelease(&view, Qt::LeftButton, {}, to);
  // Two frames, each keying the head and the layer's own move.
  QCOMPARE(bench.selection.keys().size(), size_t{4});
  QTest::mouseClick(&view, Qt::LeftButton, Qt::ControlModifier,
                    Cell(view, 10));
  QCOMPARE(bench.selection.keys().size(), size_t{2});
  QTest::mouseClick(&view, Qt::LeftButton, Qt::ShiftModifier,
                    Cell(view, 10));
  QCOMPARE(bench.selection.keys().size(), size_t{4});
  QTest::mouseClick(&view, Qt::LeftButton, {}, Cell(view, 20));
  QVERIFY(bench.selection.keys().empty());
  QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
  QCOMPARE(bench.selection.keys().size(), size_t{4});
}

void TimelineViewTests::StepModeToggles() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  auto* toggle = view.findChild<QToolButton*>("step_mode");
  QVERIFY(toggle != nullptr);
  QCOMPARE(toggle->text(), QString("Scrub mode"));
  toggle->click();
  QCOMPARE(bench.playback.step_mode(), StepMode::kAnimation);
  QCOMPARE(toggle->text(), QString("Animation mode"));
  bench.playback.Step(1);
  QCOMPARE(bench.playback.frame(), Frame(2));
}

void TimelineViewTests::KeysGrabFromTwelvePixelsAndWaitFourToSlide() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  // The key is at frame 2; press 11 pixels from it, nearer to it than
  // to any other.
  const QPoint key = Cell(view, 2);
  const QPoint near = key + QPoint(-11, 0);
  QTest::mousePress(&view, Qt::LeftButton, {}, near);
  QVERIFY(!bench.selection.keys().empty());
  // 3 pixels of drag slide nothing; 4 and a frame of travel do.
  QTest::mouseMove(&view, near + QPoint(3, 0));
  QCOMPARE(DollKeys(bench)[0], Frame(2));
  QTest::mouseMove(&view, Cell(view, 5));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 5));
  QCOMPARE(DollKeys(bench)[0], Frame(5));
}

void TimelineViewTests::KeyDragLeavesAGhostOfTheKey() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QCOMPARE(view.feedback().ghost_kind(), GhostKind::kNone);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QCOMPARE(view.feedback().ghost_kind(), GhostKind::kKey);
  QTest::mouseMove(&view, Cell(view, 6));
  QCOMPARE(view.feedback().ghost_kind(), GhostKind::kKey);
  QTest::keyClick(&view, Qt::Key_Escape);
  QCOMPARE(view.feedback().ghost_kind(), GhostKind::kNone);
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 6));
  QCOMPARE(DollKeys(bench)[0], Frame(2));
}

void TimelineViewTests::KeyDragSaysHowManyFrames() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QVERIFY(!view.feedback().is_active());
  QTest::mouseMove(&view, Cell(view, 6));
  QVERIFY(view.feedback().is_active());
  QCOMPARE(view.feedback().text(), QString("+4 f"));
  QTest::mouseMove(&view, Cell(view, 1));
  QCOMPARE(view.feedback().text(), QString("-1 f"));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 1));
  QVERIFY(!view.feedback().is_active());
  QCOMPARE(bench.history.UndoLabel(), QString("Move keys"));
}

void TimelineViewTests::EscapeCancelsTheKeyDrag() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kOff);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  const std::set<KeyRef> picked = bench.selection.keys();
  QTest::mouseMove(&view, Cell(view, 6));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(6)}));
  QTest::keyClick(&view, Qt::Key_Escape);
  QVERIFY(!view.feedback().is_active());
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2)}));
  QCOMPARE(bench.selection.keys(), picked);
  QTest::mouseMove(&view, Cell(view, 9));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 9));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2)}));
  QVERIFY(bench.history.UndoLabel() != QString("Move keys"));
}

void TimelineViewTests::ANewKeyPopsAndADeletedOnePuffs() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kFull);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mouseDClick(&view, Qt::LeftButton, {}, Cell(view, 10));
  // The key is in the data at once; only its paint pops.
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2), Frame(10)}));
  QVERIFY(view.effects().ScaleOf(1, 0, Frame(10)) > 1.0);
  QCOMPARE(view.effects().ScaleOf(1, 0, Frame(2)), 1.0);
  QTRY_VERIFY(!view.effects().IsPlaying());
  QTest::mouseClick(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::keyClick(&view, Qt::Key_Delete);
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(10)}));
  QCOMPARE(view.effects().Puffs().size(), size_t{1});
  QCOMPARE(view.effects().Puffs()[0].frame, Frame(2));
  QTRY_VERIFY(view.effects().Puffs().empty());
}

void TimelineViewTests::UndoRewindsAndTheDataIsAlreadyBack() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kFull);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mouseClick(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::keyClick(&view, Qt::Key_Delete);
  QTRY_VERIFY(!view.effects().IsPlaying());
  bench.history.Undo();
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2)}));
  QVERIFY(view.effects().GlowOf(1, 0, Frame(2)) > 0.0);
  QTRY_VERIFY(!view.effects().IsPlaying());
  bench.history.Redo();
  QVERIFY(DollKeys(bench).empty());
  QCOMPARE(view.effects().Puffs().size(), size_t{1});
}

void TimelineViewTests::ASlidKeyPopsNothingAndOffShowsNothing() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kFull);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::mouseMove(&view, Cell(view, 6));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 6));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(6)}));
  QVERIFY(!view.effects().IsPlaying());
  bench.motion.SetMode(MotionMode::kOff);
  bench.history.Undo();
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2)}));
  QVERIFY(!view.effects().IsPlaying());
}

void TimelineViewTests::UndoOfAMoveSlidesTheKeyBack() {
  Bench bench;
  Stage(&bench);
  bench.motion.SetMode(MotionMode::kFull);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::mouseMove(&view, Cell(view, 6));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 6));
  QTRY_VERIFY(!view.effects().IsPlaying());
  bench.history.Undo();
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2)}));
  QCOMPARE(view.effects().SlideOffsetOf(1, 0, Frame(2)), 4.0);
  QTRY_COMPARE(view.effects().SlideOffsetOf(1, 0, Frame(2)), 0.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::TimelineViewTests)
#include "timeline_view_tests.moc"
