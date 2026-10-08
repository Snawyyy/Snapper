#include <QTest>

#include "anim/warp_motion.h"

namespace snapper {

class WarpMotionTests final : public QObject {
  Q_OBJECT

 private slots:
  void StillWhenOff();
  void WaveRunsFromTheAnchor();
};

void WarpMotionTests::StillWhenOff() {
  const QSize size(20, 40);
  const WarpGrid grid{2, 4};
  const std::vector<QPointF> rest = MotionPushes(size, grid, {}, Frame(5));
  QCOMPARE(rest, std::vector<QPointF>(15, QPointF()));
  WarpMotion on;
  on.kind = static_cast<WarpMotionKind>(kWarpMotionKindCount - 1);
  QVERIFY(MotionPushes(size, WarpGrid(), on, Frame(5)).empty());
  QCOMPARE(MotionPushes(QSize(), grid, on, Frame(5)),
           std::vector<QPointF>(15, QPointF()));
}

// One cycle of 24 frames, 10 pixels, on a grid one cell across and two
// down: points 0-1 on top, 2-3 in the middle, 4-5 at the bottom.
WarpMotion Motion(WarpMotionKind kind, WarpEdge edge = WarpEdge::kTop) {
  WarpMotion motion;
  motion.kind = kind;
  motion.size = 10.0;
  motion.cycle = 24;
  motion.edge = edge;
  return motion;
}

void WarpMotionTests::WaveRunsFromTheAnchor() {
  const QSize size(20, 40);
  const WarpGrid grid{1, 2};
  const WarpMotion wave = Motion(WarpMotionKind::kWave);
  const std::vector<QPointF> quarter =
      MotionPushes(size, grid, wave, Frame(6));
  QCOMPARE(quarter[0], QPointF());
  QCOMPARE(quarter[1], QPointF());
  // The far end is a quarter wave behind the middle, so they lean apart.
  QCOMPARE(quarter[4].x(), 10.0);
  QCOMPARE(quarter[2].x(), -5.0);
  QCOMPARE(quarter[4].y(), 0.0);
  QCOMPARE(MotionPushes(size, grid, wave, Frame(30)), quarter);
  const std::vector<QPointF> sideways = MotionPushes(
      size, grid, Motion(WarpMotionKind::kWave, WarpEdge::kLeft), Frame(6));
  QCOMPARE(sideways[0], QPointF());
  QCOMPARE(sideways[1].x(), 0.0);
  QCOMPARE(sideways[1].y(), 10.0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::WarpMotionTests)
#include "warp_motion_tests.moc"
