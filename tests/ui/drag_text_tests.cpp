#include <QTest>

#include "ui/drag_text.h"

namespace snapper {

class DragTextTests final : public QObject {
  Q_OBJECT

 private slots:
  void AnglesAreWholeDegrees();
  void OffsetsCarryASign();
  void FramesCarryASign();
  void ScaleIsAPercentage();
  void SnapRoundsToTheStep();
  void TinyGapsAreNotSnaps();
  void ChipRidesOffTheCursor();
  void ChipStaysInsideBounds();
};

void DragTextTests::AnglesAreWholeDegrees() {
  QCOMPARE(AngleText(34.2), QStringLiteral("34 deg"));
  QCOMPARE(AngleText(-11.6), QStringLiteral("-12 deg"));
  QCOMPARE(AngleText(-0.2), QStringLiteral("0 deg"));
}

void DragTextTests::OffsetsCarryASign() {
  QCOMPARE(OffsetText(QPointF(12.4, -3.6)), QStringLiteral("+12, -4 px"));
  QCOMPARE(OffsetText(QPointF(0.0, 0.0)), QStringLiteral("0, 0 px"));
}

void DragTextTests::FramesCarryASign() {
  QCOMPARE(FramesText(3), QStringLiteral("+3 f"));
  QCOMPARE(FramesText(-2), QStringLiteral("-2 f"));
  QCOMPARE(FramesText(0), QStringLiteral("0 f"));
}

void DragTextTests::ScaleIsAPercentage() {
  QCOMPARE(ScaleText(1.5), QStringLiteral("150 %"));
  QCOMPARE(ScaleText(0.5), QStringLiteral("50 %"));
}

void DragTextTests::SnapRoundsToTheStep() {
  QCOMPARE(SnapDegrees(22.0, kTurnSnapDegrees), 15.0);
  QCOMPARE(SnapDegrees(23.0, kTurnSnapDegrees), 30.0);
  QCOMPARE(SnapDegrees(-8.0, kTurnSnapDegrees), -15.0);
  QCOMPARE(SnapDegrees(0.0, kTurnSnapDegrees), 0.0);
}

void DragTextTests::TinyGapsAreNotSnaps() {
  QVERIFY(DidSnap(22.0, 15.0));
  QVERIFY(!DidSnap(15.001, 15.0));
  QVERIFY(!DidSnap(30.0, 30.0));
}

void DragTextTests::ChipRidesOffTheCursor() {
  const QRectF bounds(0, 0, 400, 300);
  const QRectF chip = ChipRect(QPointF(100, 50), QSizeF(60, 20), bounds);
  QCOMPARE(chip.topLeft(), QPointF(100 + kChipOffset, 50 + kChipOffset));
  QCOMPARE(chip.size(), QSizeF(60, 20));
}

void DragTextTests::ChipStaysInsideBounds() {
  const QRectF bounds(0, 0, 400, 300);
  const QRectF chip = ChipRect(QPointF(390, 295), QSizeF(60, 20), bounds);
  QVERIFY(bounds.contains(chip));
  const QRectF tiny = ChipRect(QPointF(5, 5), QSizeF(60, 20),
                               QRectF(0, 0, 30, 10));
  QCOMPARE(tiny.topLeft(), QPointF(0, 0));
}

}  // namespace snapper

QTEST_MAIN(snapper::DragTextTests)
#include "drag_text_tests.moc"
