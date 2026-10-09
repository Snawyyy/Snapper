#include <QTest>

#include "ui/grab.h"

namespace snapper {

class GrabTests final : public QObject {
  Q_OBJECT

 private slots:
  void ConstantsAreTheDesign();
  void NearestHandleWins();
  void ReachEdgeCounts();
  void NothingInReachIsNoHit();
  void ATieGoesToTheFirst();
  void DeadZoneHoldsUntilFourPixels();
  void GraceKeepsHiddenHandlesFor100Ms();
  void GraceEndsWhenHandlesShowAgain();
  void EachKindHasItsCursor();
};

void GrabTests::ConstantsAreTheDesign() {
  QCOMPARE(kHandleDraw, 8.0);
  QCOMPARE(kGrabReach, 12.0);
  QCOMPARE(kDragDeadZone, 4.0);
  QCOMPARE(kGraceMs, 100);
  QCOMPARE(kHoverGrow, 1.15);
}

void GrabTests::NearestHandleWins() {
  const std::vector<QPointF> spots = {{0, 0}, {20, 0}, {9, 0}};
  QCOMPARE(NearestGrab(spots, QPointF(10, 0)), 2);
  QCOMPARE(NearestGrab(spots, QPointF(18, 0)), 1);
  QCOMPARE(NearestGrab(spots, QPointF(1, 1)), 0);
}

void GrabTests::ReachEdgeCounts() {
  const std::vector<QPointF> spots = {{0, 0}};
  QCOMPARE(NearestGrab(spots, QPointF(kGrabReach, 0)), 0);
  QCOMPARE(NearestGrab(spots, QPointF(kGrabReach + 0.01, 0)), -1);
  QCOMPARE(NearestGrab(spots, QPointF(0, -kGrabReach)), 0);
  QCOMPARE(NearestGrab(spots, QPointF(5, 0), 4.0), -1);
  QCOMPARE(NearestGrab(spots, QPointF(4, 0), 4.0), 0);
}

void GrabTests::NothingInReachIsNoHit() {
  QCOMPARE(NearestGrab({}, QPointF(0, 0)), -1);
  const std::vector<QPointF> spots = {{100, 100}, {-50, 3}};
  QCOMPARE(NearestGrab(spots, QPointF(0, 0)), -1);
}

void GrabTests::ATieGoesToTheFirst() {
  const std::vector<QPointF> spots = {{-5, 0}, {5, 0}};
  QCOMPARE(NearestGrab(spots, QPointF(0, 0)), 0);
  QCOMPARE(NearestGrab(spots, QPointF(1, 0)), 1);
}

void GrabTests::DeadZoneHoldsUntilFourPixels() {
  const QPointF press(50, 50);
  QVERIFY(!IsPastDeadZone(press, press));
  QVERIFY(!IsPastDeadZone(press, QPointF(53.9, 50)));
  QVERIFY(IsPastDeadZone(press, QPointF(54, 50)));
  QVERIFY(IsPastDeadZone(press, QPointF(50, 46)));
  // Across the diagonal: 3 and 3 is 4.24 apart.
  QVERIFY(IsPastDeadZone(press, QPointF(53, 53)));
  QVERIFY(!IsPastDeadZone(press, QPointF(52, 52)));
}

void GrabTests::GraceKeepsHiddenHandlesFor100Ms() {
  HandleGrace grace;
  QVERIFY(!grace.IsGrabbable(false, 0));
  grace.Update(true, 1000);
  QVERIFY(grace.IsGrabbable(true, 1000));
  grace.Update(false, 2000);
  QVERIFY(grace.IsGrabbable(false, 2000));
  QVERIFY(grace.IsGrabbable(false, 2100));
  QVERIFY(!grace.IsGrabbable(false, 2101));
  QVERIFY(!grace.IsGrabbable(false, 9000));
}

void GrabTests::GraceEndsWhenHandlesShowAgain() {
  HandleGrace grace;
  grace.Update(true, 10);
  grace.Update(false, 20);
  // Hiding twice in a row does not restart the clock.
  grace.Update(false, 90);
  QVERIFY(!grace.IsGrabbable(false, 121));
  grace.Update(true, 130);
  grace.Update(false, 140);
  QVERIFY(grace.IsGrabbable(false, 240));
  QVERIFY(!grace.IsGrabbable(false, 241));
}

void GrabTests::EachKindHasItsCursor() {
  QCOMPARE(CursorFor(GrabKind::kNone), Qt::ArrowCursor);
  QCOMPARE(CursorFor(GrabKind::kLean), Qt::SizeVerCursor);
  QCOMPARE(CursorFor(GrabKind::kSwivel), Qt::SizeHorCursor);
  QCOMPARE(CursorFor(GrabKind::kIkTip), Qt::CrossCursor);
  QCOMPARE(CursorFor(GrabKind::kPivot), Qt::SizeAllCursor);
  QVERIFY(CursorFor(GrabKind::kWarpDot) != CursorFor(GrabKind::kPivot));
}

}  // namespace snapper

QTEST_MAIN(snapper::GrabTests)
#include "grab_tests.moc"
