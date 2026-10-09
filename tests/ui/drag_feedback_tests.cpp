#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

#include "bench.h"
#include "ui/drag_feedback.h"

namespace snapper {

class DragFeedbackTests final : public QObject {
  Q_OBJECT

 private slots:
  void StartsQuiet();
  void GhostStaysAtTheStart();
  void ChipFollowsTheCursor();
  void SnapFlashes();
  void StayingOnASnapDoesNotFlashAgain();
  void FlashEasesOutInFull();
  void SettlePopsThenGoes();
  void SettleIsInstantInOff();
  void CancelClearsEverything();
  void PaintsTheChip();
  void PulseShowsThenFadesAfterARest();
  void NewPulseKeepsTheChipUp();
  void CancelClearsAPulse();
};

void DragFeedbackTests::StartsQuiet() {
  Bench bench;
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  QVERIFY(!feedback.is_active());
  QCOMPARE(feedback.flash(), 0.0);
  QCOMPARE(feedback.settle(), 0.0);
}

void DragFeedbackTests::GhostStaysAtTheStart() {
  Bench bench;
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(10, 10), QPointF(20, 20), "a", false);
  feedback.Update(QPointF(99, 99), QPointF(40, 30), "b", false);
  QVERIFY(feedback.is_active());
  QCOMPARE(feedback.ghost(), QPointF(10, 10));
}

void DragFeedbackTests::ChipFollowsTheCursor() {
  Bench bench;
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "+1 f", false);
  feedback.Update(QPointF(0, 0), QPointF(8, 9), "+2 f", false);
  QCOMPARE(feedback.text(), QStringLiteral("+2 f"));
  QCOMPARE(feedback.cursor(), QPointF(8, 9));
}

void DragFeedbackTests::SnapFlashes() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "10 deg", false);
  QCOMPARE(feedback.flash(), 0.0);
  feedback.Update(QPointF(0, 0), QPointF(6, 5), "15 deg", true);
  QVERIFY(feedback.flash() > 0.0);
}

void DragFeedbackTests::StayingOnASnapDoesNotFlashAgain() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "15 deg", true);
  bench.motion.SetMode(MotionMode::kFull);
  feedback.Update(QPointF(0, 0), QPointF(6, 5), "15 deg", true);
  QCOMPARE(feedback.flash(), 0.0);
  feedback.Update(QPointF(0, 0), QPointF(9, 5), "30 deg", true);
  QVERIFY(feedback.flash() > 0.0);
}

void DragFeedbackTests::FlashEasesOutInFull() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "15 deg", true);
  QVERIFY(feedback.flash() > 0.0);
  QTRY_COMPARE(feedback.flash(), 0.0);
}

void DragFeedbackTests::SettlePopsThenGoes() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "+3 f", false);
  feedback.Settle();
  QVERIFY(!feedback.is_active());
  QVERIFY(feedback.text().isEmpty());
  QVERIFY(feedback.settle() != 0.0);
  QTRY_COMPARE(feedback.settle(), 0.0);
}

void DragFeedbackTests::SettleIsInstantInOff() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "+3 f", true);
  QCOMPARE(feedback.flash(), 0.0);
  feedback.Settle();
  QCOMPARE(feedback.settle(), 0.0);
  QVERIFY(!feedback.is_active());
}

void DragFeedbackTests::CancelClearsEverything() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Update(QPointF(0, 0), QPointF(5, 5), "15 deg", true);
  QSignalSpy changed(&feedback, &DragFeedback::Changed);
  feedback.Cancel();
  QVERIFY(!feedback.is_active());
  QCOMPARE(feedback.flash(), 0.0);
  QCOMPARE(feedback.settle(), 0.0);
  QVERIFY(feedback.text().isEmpty());
  QVERIFY(!changed.isEmpty());
  // A fresh drag starts a fresh ghost.
  feedback.Update(QPointF(50, 60), QPointF(55, 65), "x", false);
  QCOMPARE(feedback.ghost(), QPointF(50, 60));
}

void DragFeedbackTests::PaintsTheChip() {
  Bench bench;
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  QImage image(200, 100, QImage::Format_ARGB32);
  image.fill(Qt::black);
  const QImage blank = image;
  feedback.Update(QPointF(20, 20), QPointF(60, 40), "34 deg", false);
  QPainter painter(&image);
  feedback.Paint(&painter, QRectF(0, 0, 200, 100));
  painter.end();
  QVERIFY(image != blank);
  QVERIFY(feedback.is_active());
}

void DragFeedbackTests::PulseShowsThenFadesAfterARest() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Pulse(QPointF(5, 5), "-10 deg");
  QCOMPARE(feedback.pulse(), 1.0);
  QCOMPARE(feedback.pulse_text(), QStringLiteral("-10 deg"));
  QVERIFY(!feedback.is_active());
  QTRY_COMPARE_WITH_TIMEOUT(feedback.pulse(), 0.0, kRestMs + 2000);
}

void DragFeedbackTests::NewPulseKeepsTheChipUp() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Pulse(QPointF(5, 5), "-5 deg");
  QTest::qWait(kRestMs / 2);
  feedback.Pulse(QPointF(5, 5), "-10 deg");
  QTest::qWait(kRestMs / 2 + 100);
  QCOMPARE(feedback.pulse(), 1.0);
  QCOMPARE(feedback.pulse_text(), QStringLiteral("-10 deg"));
  QTRY_COMPARE_WITH_TIMEOUT(feedback.pulse(), 0.0, kRestMs + 1000);
}

void DragFeedbackTests::CancelClearsAPulse() {
  Bench bench;
  QObject owner;
  DragFeedback feedback(&bench.motion, &owner);
  feedback.Pulse(QPointF(5, 5), "-5 deg");
  QCOMPARE(feedback.pulse(), 1.0);
  feedback.Cancel();
  QCOMPARE(feedback.pulse(), 0.0);
  QTest::qWait(kRestMs + 100);
  QCOMPARE(feedback.pulse(), 0.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::DragFeedbackTests)
#include "drag_feedback_tests.moc"
