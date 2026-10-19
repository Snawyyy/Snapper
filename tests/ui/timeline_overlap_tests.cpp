#include <QImage>
#include <QTest>
#include <QWheelEvent>

#include <cassert>

#include "anim/reel_timeline.h"
#include "bench.h"
#include "ui/reel_timeline.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

const ShotId kShot(1);

// One character layer with a key at frame 2, in a 48-frame shot.
void Stage(Bench* bench) {
  assert(bench != nullptr);
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Bob";
  layer.content = CharacterLayer{"Bob", {}, false};
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  QVERIFY(bench->pose.KeyInPlace(kShot, LayerId(1), Frame(2)).has_value());
  bench->motion.SetMode(MotionMode::kOff);
}

QPoint Cell(const TimelineView& view, int frame) {
  assert(frame >= 0);
  assert(view.XOf(Frame(frame)) > -1000.0);
  return QPoint(static_cast<int>(view.XOf(Frame(frame)) +
                                 kDefaultFrameWidth / 2.0),
                kRulerHeight + kWaveHeight + kRowHeight / 2);
}

void ScrollRight(TimelineView* view, int notches) {
  assert(view != nullptr);
  assert(notches > 0);
  QWheelEvent wheel(QPointF(200, 60), QPointF(200, 60), QPoint(),
                    QPoint(0, -120 * notches), Qt::NoButton, {},
                    Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(view, &wheel);
}

}  // namespace

class TimelineOverlapTests final : public QObject {
  Q_OBJECT

 private slots:
  void ScrolledKeysAndPlayheadStayOffTheNames();
  void KeyDragCarriesThePlayheadAndEscapeBringsItBack();
  void KeyDragReleaseLeavesThePlayheadAndNoHistory();
  void ClipDragCarriesThePlayheadAndEscapeBringsItBack();
};

void TimelineOverlapTests::ScrolledKeysAndPlayheadStayOffTheNames() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  bench.playback.Seek(Frame(2));
  ScrollRight(&view, 1);
  // Frame 2 is now left of the frame area: inside the names.
  QVERIFY(view.XOf(Frame(2)) < kNameWidth);
  const QImage image = view.grab().toImage();
  const int middle = kRulerHeight + kWaveHeight + kRowHeight / 2;
  const int key_x = static_cast<int>(view.XOf(Frame(2)) +
                                     kDefaultFrameWidth / 2.0);
  QVERIFY(key_x > 40 && key_x < kNameWidth - 4);
  const QColor bare = image.pixelColor(kNameWidth - 6, middle);
  for (int dx = -8; dx <= 8; ++dx) {
    QCOMPARE(image.pixelColor(key_x + dx, middle), bare);
  }
  QCOMPARE(image.pixelColor(key_x, middle + 8), bare);
}

void TimelineOverlapTests::KeyDragCarriesThePlayheadAndEscapeBringsItBack() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  bench.playback.Seek(Frame(20));
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::mouseMove(&view, Cell(view, 6));
  QCOMPARE(bench.playback.frame(), Frame(6));
  QTest::mouseMove(&view, Cell(view, 9));
  QCOMPARE(bench.playback.frame(), Frame(9));
  QTest::keyClick(&view, Qt::Key_Escape);
  QCOMPARE(bench.playback.frame(), Frame(20));
  QTest::mouseMove(&view, Cell(view, 12));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 12));
  QCOMPARE(bench.playback.frame(), Frame(20));
}

void TimelineOverlapTests::KeyDragReleaseLeavesThePlayheadAndNoHistory() {
  Bench bench;
  Stage(&bench);
  TimelineView view(bench.All());
  view.resize(800, 200);
  QTest::mousePress(&view, Qt::LeftButton, {}, Cell(view, 2));
  QTest::mouseMove(&view, Cell(view, 7));
  QTest::mouseRelease(&view, Qt::LeftButton, {}, Cell(view, 7));
  QCOMPARE(bench.playback.frame(), Frame(7));
  QCOMPARE(bench.history.UndoLabel(), QString("Move keys"));
  bench.history.Undo();
  QVERIFY(bench.history.UndoLabel() != QString("Move keys"));
}

void TimelineOverlapTests::ClipDragCarriesThePlayheadAndEscapeBringsItBack() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  bench.playback.SetTimeline(Timeline::kReel);
  const ClipId clip = *bench.reel.AddShot(ShotId(1), 0, Frame(0));
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  bench.playback.Seek(Frame(10));
  const auto at = [&](int frame) {
    return QPoint(static_cast<int>(timeline.XOf(Frame(frame))),
                  static_cast<int>(timeline.TrackTop(0)) + 10);
  };
  QTest::mousePress(&timeline, Qt::LeftButton, {}, at(24));
  QTest::mouseMove(&timeline, at(36));
  QTest::mouseMove(&timeline, at(48));
  QCOMPARE(ClipOf(bench.history.current().reel, clip)->start, Frame(24));
  QCOMPARE(bench.playback.frame(), Frame(24));
  QTest::keyClick(&timeline, Qt::Key_Escape);
  QCOMPARE(bench.playback.frame(), Frame(10));
  QTest::mouseRelease(&timeline, Qt::LeftButton, {}, at(48));
  QCOMPARE(bench.playback.frame(), Frame(10));
}

}  // namespace snapper

QTEST_MAIN(snapper::TimelineOverlapTests)
#include "timeline_overlap_tests.moc"
