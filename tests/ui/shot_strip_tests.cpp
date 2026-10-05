#include <QTest>

#include "bench.h"
#include "ui/shot_strip.h"

namespace snapper {
namespace {

QPoint Middle(const QRectF& rect) { return rect.center().toPoint(); }

QStringList Names(const Bench& bench) {
  QStringList names;
  for (const auto& shot : bench.history.current().shots) {
    names.append(shot->name);
  }
  return names;
}

}  // namespace

class ShotStripTests final : public QObject {
  Q_OBJECT

 private slots:
  void PlusAddsAfterThePickedShot();
  void ClickPicksAndSeeks();
  void DraggingTheEdgeChangesLength();
  void DraggingAShotReordersIt();
  void DeleteRemovesThePickedShot();
};

void ShotStripTests::PlusAddsAfterThePickedShot() {
  Bench bench;
  ShotStrip strip(bench.All());
  strip.resize(800, 40);
  QTest::mouseClick(&strip, Qt::LeftButton, {}, Middle(strip.AddBlock()));
  QTest::mouseClick(&strip, Qt::LeftButton, {}, Middle(strip.AddBlock()));
  QCOMPARE(Names(bench), QStringList({"Shot 1", "Shot 2"}));
  QCOMPARE(bench.selection.shot(), ShotId(2));
  QTest::mouseClick(&strip, Qt::LeftButton, {},
                    Middle(strip.Blocks()[0].rect));
  QTest::mouseClick(&strip, Qt::LeftButton, {}, Middle(strip.AddBlock()));
  QCOMPARE(Names(bench), QStringList({"Shot 1", "Shot 3", "Shot 2"}));
}

void ShotStripTests::ClickPicksAndSeeks() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  QVERIFY(bench.shots.Add(-1).has_value());
  ShotStrip strip(bench.All());
  strip.resize(800, 40);
  QTest::mouseClick(&strip, Qt::LeftButton, {},
                    Middle(strip.Blocks()[1].rect));
  QCOMPARE(bench.selection.shot(), ShotId(2));
  QCOMPARE(bench.playback.frame(), Frame(48));
}

void ShotStripTests::DraggingTheEdgeChangesLength() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  ShotStrip strip(bench.All());
  strip.resize(800, 40);
  const QRectF block = strip.Blocks()[0].rect;
  const QPoint edge(static_cast<int>(block.right()) - 2,
                    static_cast<int>(block.center().y()));
  QTest::mousePress(&strip, Qt::LeftButton, {}, edge);
  QTest::mouseMove(&strip, edge + QPoint(24, 0));
  QTest::mouseRelease(&strip, Qt::LeftButton, {}, edge + QPoint(24, 0));
  QCOMPARE(bench.history.current().shots[0]->length, Frame(60));
  QCOMPARE(bench.history.UndoLabel(), QString("Change shot length"));
}

void ShotStripTests::DraggingAShotReordersIt() {
  Bench bench;
  for (int i = 0; i < 3; ++i) {
    QVERIFY(bench.shots.Add(-1).has_value());
  }
  ShotStrip strip(bench.All());
  strip.resize(800, 40);
  const QPoint first = Middle(strip.Blocks()[0].rect);
  const QPoint last = Middle(strip.Blocks()[2].rect) + QPoint(10, 0);
  QTest::mousePress(&strip, Qt::LeftButton, {}, first);
  QTest::mouseMove(&strip, last);
  QTest::mouseRelease(&strip, Qt::LeftButton, {}, last);
  QCOMPARE(Names(bench), QStringList({"Shot 2", "Shot 3", "Shot 1"}));
}

void ShotStripTests::DeleteRemovesThePickedShot() {
  Bench bench;
  QVERIFY(bench.shots.Add(-1).has_value());
  QVERIFY(bench.shots.Add(-1).has_value());
  ShotStrip strip(bench.All());
  strip.resize(800, 40);
  QTest::mouseClick(&strip, Qt::LeftButton, {},
                    Middle(strip.Blocks()[0].rect));
  QTest::keyClick(&strip, Qt::Key_Delete);
  QCOMPARE(Names(bench), QStringList({"Shot 2"}));
}

}  // namespace snapper

QTEST_MAIN(snapper::ShotStripTests)
#include "shot_strip_tests.moc"
