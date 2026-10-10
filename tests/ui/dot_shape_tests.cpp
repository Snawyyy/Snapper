#include <QTest>

#include "ui/dot_shape.h"

namespace snapper {

class DotShapeTests final : public QObject {
  Q_OBJECT

 private slots:
  void EachKindHasItsShape();
  void TailPointsBackToTheKeyedPlace();
  void TailIsShortAndNoneAtRest();
  void BlobStretchesAlongMotionKeepingArea();
  void BlobIsRoundAtRestOrWithMotionOff();
};

void DotShapeTests::EachKindHasItsShape() {
  QCOMPARE(ShapeOf(std::nullopt), DotShape::kPlain);
  QCOMPARE(ShapeOf(NodeKind::kDrag), DotShape::kRing);
  QCOMPARE(ShapeOf(NodeKind::kCloth), DotShape::kDiamond);
  QCOMPARE(ShapeOf(NodeKind::kJelly), DotShape::kBlob);
}

void DotShapeTests::TailPointsBackToTheKeyedPlace() {
  const QPointF tail = TailOf(QPointF(6.0, 0.0));
  QCOMPARE(tail, QPointF(-6.0, 0.0));
  QVERIFY(TailOf(QPointF(0.0, -4.0)).y() > 0.0);
}

void DotShapeTests::TailIsShortAndNoneAtRest() {
  QCOMPARE(TailOf(QPointF(0.5, 0.0)), QPointF());
  const QPointF far = TailOf(QPointF(100.0, 0.0));
  QCOMPARE(far, QPointF(-kTailMax, 0.0));
}

void DotShapeTests::BlobStretchesAlongMotionKeepingArea() {
  const Squish squish = SquishOf(QPointF(0.0, -12.0), true);
  QVERIFY(squish.along > 1.0);
  QVERIFY(squish.across < 1.0);
  QVERIFY(qAbs(squish.along * squish.across - 1.0) < 1e-9);
  QCOMPARE(squish.angle, -90.0);
  const Squish more = SquishOf(QPointF(0.0, -100.0), true);
  QCOMPARE(more.along, squish.along);
}

void DotShapeTests::BlobIsRoundAtRestOrWithMotionOff() {
  QCOMPARE(SquishOf(QPointF(), true).along, 1.0);
  QCOMPARE(SquishOf(QPointF(9.0, 0.0), false).along, 1.0);
  QCOMPARE(SquishOf(QPointF(9.0, 0.0), false).across, 1.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::DotShapeTests)
#include "dot_shape_tests.moc"
