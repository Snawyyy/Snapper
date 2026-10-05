#include <QTest>

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

}  // namespace snapper

QTEST_MAIN(snapper::TimelineViewTests)
#include "timeline_view_tests.moc"
