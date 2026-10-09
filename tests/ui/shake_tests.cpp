#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QToolTip>

#include <cassert>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/refusal_shaker.h"

namespace snapper {

class ShakeTests final : public QObject {
  Q_OBJECT

 private slots:
  void OffsetStartsAndEndsAtRest();
  void OffsetJerksBothWaysAndFades();
  void ClickingAGreyedControlShakesItAndComesHome();
  void OffShowsTheReasonAndDoesNotMove();
  void AnEnabledControlDoesNotShake();
  void ASilentGreyedControlShakesWithAGenericReason();
};

namespace {

// A greyed button with a reason, in a shown window.
struct Refused final {
  Bench rig;
  MainWindow window{rig.All()};
  QPushButton button{"Copy", &window};

  Refused(bool has_reason) {
    assert(window.width() > 0);
    assert(button.parent() == &window);
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    button.move(100, 100);
    button.show();
    button.setEnabled(false);
    button.setToolTip(has_reason ? "Pick a doll first." : QString());
  }
};

}  // namespace

void ShakeTests::OffsetStartsAndEndsAtRest() {
  QCOMPARE(ShakeOffsetPx(0.0), 0.0);
  QCOMPARE(ShakeOffsetPx(1.0), 0.0);
  QCOMPARE(ShakeOffsetPx(-1.0), 0.0);
  QCOMPARE(ShakeOffsetPx(2.0), 0.0);
}

void ShakeTests::OffsetJerksBothWaysAndFades() {
  QCOMPARE(ShakeOffsetPx(0.2), -kShakePx);
  QCOMPARE(ShakeOffsetPx(0.4), kShakePx);
  QVERIFY(ShakeOffsetPx(0.6) < 0.0);
  QVERIFY(ShakeOffsetPx(0.8) > 0.0);
  QVERIFY(qAbs(ShakeOffsetPx(0.6)) < kShakePx);
  QVERIFY(qAbs(ShakeOffsetPx(0.8)) < qAbs(ShakeOffsetPx(0.6)));
  for (int step = 0; step <= 100; ++step) {
    QVERIFY(qAbs(ShakeOffsetPx(step / 100.0)) <= kShakePx);
  }
}

void ShakeTests::ClickingAGreyedControlShakesItAndComesHome() {
  Refused refused(true);
  refused.rig.motion.SetMode(MotionMode::kFull);
  const QPoint home = refused.button.pos();
  QTest::mouseClick(&refused.button, Qt::LeftButton);
  QTRY_VERIFY(refused.button.pos() != home);
  QTRY_COMPARE(refused.button.pos(), home);
}

void ShakeTests::OffShowsTheReasonAndDoesNotMove() {
  Refused refused(true);
  refused.rig.motion.SetMode(MotionMode::kOff);
  const QPoint home = refused.button.pos();
  QTest::mouseClick(&refused.button, Qt::LeftButton);
  QCOMPARE(refused.button.pos(), home);
  QTRY_VERIFY(QToolTip::isVisible());
  QCOMPARE(QToolTip::text(), QString("Pick a doll first."));
}

void ShakeTests::AnEnabledControlDoesNotShake() {
  Refused refused(true);
  refused.rig.motion.SetMode(MotionMode::kFull);
  const QPoint home = refused.button.pos();
  refused.button.setEnabled(true);
  QTest::mouseClick(&refused.button, Qt::LeftButton);
  QTest::qWait(60);
  QCOMPARE(refused.button.pos(), home);
  QVERIFY(refused.button.isEnabled());
}

void ShakeTests::ASilentGreyedControlShakesWithAGenericReason() {
  Refused silent(false);
  silent.rig.motion.SetMode(MotionMode::kFull);
  const QPoint home = silent.button.pos();
  QTest::mouseClick(&silent.button, Qt::LeftButton);
  QTRY_VERIFY(silent.button.pos() != home);
  QTRY_COMPARE(silent.button.pos(), home);
  QTRY_VERIFY(QToolTip::isVisible());
  QCOMPARE(QToolTip::text(), QString("Not available right now"));
}

}  // namespace snapper

QTEST_MAIN(snapper::ShakeTests)
#include "shake_tests.moc"
