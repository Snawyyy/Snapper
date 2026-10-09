#include <QTest>

#include <cassert>

#include "ui/drag_text.h"

namespace snapper {

class DragTextTests final : public QObject {
  Q_OBJECT

 private slots:
  void AnglesAreWholeDegrees();
  void OffsetsCarryASign();
  void FramesCarryASign();
  void ScaleIsAPercentage();
  void PositionCountsFromOne();
  void SnapRoundsToTheStep();
  void TinyGapsAreNotSnaps();
  void ChipSitsAboveLeftOfTheCursor();
  void ChipFlipsAndStaysInsideNearEveryEdge();
  void ChipFlipsRightWhenLeftIsShort();
  void ChipFitsATinyCanvas();
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

void DragTextTests::PositionCountsFromOne() {
  QCOMPARE(PositionText(1), QStringLiteral("Place 1"));
  QCOMPARE(PositionText(12), QStringLiteral("Place 12"));
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

namespace {

// The box the arrow cursor covers: down and right of the hotspot,
// plus the same box centered on it, to be safe for other cursors.
bool HitsCursor(const QRectF& chip, QPointF cursor) {
  assert(chip.isValid());
  assert(kCursorBox > 0.0);
  const QRectF down_right(cursor, QSizeF(kCursorBox, kCursorBox));
  const QRectF centered(cursor - QPointF(kCursorBox, kCursorBox) / 2.0,
                        QSizeF(kCursorBox, kCursorBox));
  return chip.intersects(down_right) || chip.intersects(centered);
}

}  // namespace

void DragTextTests::ChipSitsAboveLeftOfTheCursor() {
  const QRectF bounds(0, 0, 400, 300);
  const QRectF chip = ChipRect(QPointF(200, 150), QSizeF(60, 20), bounds);
  QCOMPARE(chip.bottomRight(),
           QPointF(200 - kChipGap, 150 - kChipGap));
  QCOMPARE(chip.size(), QSizeF(60, 20));
}

void DragTextTests::ChipFlipsAndStaysInsideNearEveryEdge() {
  const QRectF bounds(0, 0, 400, 300);
  const QSizeF size(60, 20);
  const QPointF spots[] = {{5, 150},   {395, 150}, {200, 5},   {200, 295},
                           {5, 5},     {395, 5},   {5, 295},   {395, 295},
                           {30, 150},  {200, 30},  {200, 150}};
  for (const QPointF& spot : spots) {
    const QRectF chip = ChipRect(spot, size, bounds);
    QVERIFY(bounds.contains(chip));
    QVERIFY2(!HitsCursor(chip, spot), "chip under the cursor");
  }
}

void DragTextTests::ChipFlipsRightWhenLeftIsShort() {
  const QRectF bounds(0, 0, 400, 300);
  const QRectF chip = ChipRect(QPointF(20, 150), QSizeF(60, 20), bounds);
  QCOMPARE(chip.left(), 20 + kChipGap);
  QVERIFY(chip.bottom() <= 150 - kChipGap);
}

void DragTextTests::ChipFitsATinyCanvas() {
  const QRectF tiny = ChipRect(QPointF(5, 5), QSizeF(60, 20),
                               QRectF(0, 0, 30, 10));
  QCOMPARE(tiny.topLeft(), QPointF(0, 0));
  QCOMPARE(tiny.size(), QSizeF(60, 20));
}

}  // namespace snapper

QTEST_MAIN(snapper::DragTextTests)
#include "drag_text_tests.moc"
