#include <QSignalSpy>
#include <QTest>

#include "bench.h"
#include "ui/handle_hover.h"

namespace snapper {

class HandleHoverTests final : public QObject {
  Q_OBJECT

 private slots:
  void GrowsTheHoveredHandleOnly();
  void LeavingShrinksAtOnce();
  void OffGrowsInstantly();
  void FullEasesToTheGrowth();
};

void HandleHoverTests::GrowsTheHoveredHandleOnly() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  HandleHover hover(&bench.motion, &owner);
  QCOMPARE(hover.ScaleOf(GrabKind::kIkTip, "arm"), 1.0);
  hover.Set(GrabKind::kIkTip, "arm");
  QCOMPARE(hover.ScaleOf(GrabKind::kIkTip, "arm"), kHoverGrow);
  QCOMPARE(hover.ScaleOf(GrabKind::kIkTip, "leg"), 1.0);
  QCOMPARE(hover.ScaleOf(GrabKind::kPivot, "arm"), 1.0);
}

void HandleHoverTests::LeavingShrinksAtOnce() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  HandleHover hover(&bench.motion, &owner);
  hover.Set(GrabKind::kPivot, "head");
  hover.Set(GrabKind::kNone, QString());
  QCOMPARE(hover.kind(), GrabKind::kNone);
  QCOMPARE(hover.ScaleOf(GrabKind::kPivot, "head"), 1.0);
}

void HandleHoverTests::OffGrowsInstantly() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kOff);
  QObject owner;
  HandleHover hover(&bench.motion, &owner);
  QSignalSpy changed(&hover, &HandleHover::Changed);
  hover.Set(GrabKind::kLean, "1");
  QVERIFY(!changed.isEmpty());
  QCOMPARE(hover.ScaleOf(GrabKind::kLean, "1"), kHoverGrow);
  const auto count = changed.size();
  hover.Set(GrabKind::kLean, "1");
  QCOMPARE(changed.size(), count);
}

void HandleHoverTests::FullEasesToTheGrowth() {
  Bench bench;
  bench.motion.SetMode(MotionMode::kFull);
  QObject owner;
  HandleHover hover(&bench.motion, &owner);
  hover.Set(GrabKind::kWarpDot, "a");
  QVERIFY(hover.ScaleOf(GrabKind::kWarpDot, "a") < kHoverGrow);
  QTRY_COMPARE(hover.ScaleOf(GrabKind::kWarpDot, "a"), kHoverGrow);
}

}  // namespace snapper

QTEST_MAIN(snapper::HandleHoverTests)
#include "handle_hover_tests.moc"
