#include <QSignalSpy>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/stage_flash.h"

namespace snapper {
namespace {

QImage Square() {
  QImage picture(8, 8, QImage::Format_ARGB32_Premultiplied);
  picture.fill(Qt::green);
  assert(!picture.isNull());
  assert(picture.width() == 8);
  return picture;
}

}  // namespace

class StageFlashTests final : public QObject {
  Q_OBJECT

 private slots:
  void AShapeFlashesThenGoes();
  void NothingToShowDoesNotFlash();
  void OffShowsNothing();
  void ClearStopsItAtOnce();
};

void StageFlashTests::AShapeFlashesThenGoes() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  StageFlash flash(&bench.motion, &owner);
  QSignalSpy changed(&flash, &StageFlash::Changed);
  flash.Show(Square(), Square(), QPointF());
  QVERIFY(flash.IsPlaying());
  QVERIFY(flash.glow() > 0.0);
  QTRY_VERIFY(!flash.IsPlaying());
  QVERIFY(!changed.isEmpty());
}

void StageFlashTests::NothingToShowDoesNotFlash() {
  Bench bench;
  QObject owner;
  StageFlash flash(&bench.motion, &owner);
  flash.Show(QImage(), QImage(), QPointF());
  QVERIFY(!flash.IsPlaying());
  QCOMPARE(flash.glow(), 0.0);
}

void StageFlashTests::OffShowsNothing() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  StageFlash flash(&bench.motion, &owner);
  flash.Show(Square(), Square(), QPointF());
  QVERIFY(!flash.IsPlaying());
  QCOMPARE(flash.glow(), 0.0);
}

void StageFlashTests::ClearStopsItAtOnce() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  StageFlash flash(&bench.motion, &owner);
  flash.Show(Square(), Square(), QPointF());
  QVERIFY(flash.IsPlaying());
  flash.Clear();
  QVERIFY(!flash.IsPlaying());
}

}  // namespace snapper

QTEST_MAIN(snapper::StageFlashTests)
#include "stage_flash_tests.moc"
