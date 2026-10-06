#include <QTest>
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
  return QPoint(static_cast<int>(view.XOf(Frame(frame)) +
                                 kDefaultFrameWidth / 2.0),
                kRulerHeight + kWaveHeight + kRowHeight / 2);
}

std::vector<Frame> DollKeys(const Bench& bench) {
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
  void StepModeToggles();
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
  QTest::mouseDClick(&view, Qt::LeftButton, {}, Cell(view, 10));
  QCOMPARE(DollKeys(bench), std::vector<Frame>({Frame(2), Frame(10)}));
}

void TimelineViewTests::DeleteRemovesPickedKeys() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mouseClick(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::keyClick(&view, Qt::Key_Delete);
  QVERIFY(DollKeys(bench).empty());
}

void TimelineViewTests::RulerClickSeeks() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
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
  const QPoint from = Cell(view, 1) + QPoint(0, -5);
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

}  // namespace snapper

QTEST_MAIN(snapper::TimelineViewTests)
#include "timeline_view_tests.moc"
