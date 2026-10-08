#include <QTest>

#include <cassert>
#include <cmath>

#include "anim/warp_motion.h"

namespace snapper {
namespace {

// One cycle of 24 frames and 10 pixels, moving right, on point 4 (the
// middle of a 2 by 2 grid).
PointMotion Motion(WarpMotionKind kind) {
  // Every test here checks a motion that moves.
  assert(kind != WarpMotionKind::kNone);
  assert(static_cast<int>(kind) < kWarpMotionKindCount);
  PointMotion motion;
  motion.point = 4;
  motion.kind = kind;
  motion.size = 10.0;
  motion.cycle = 24;
  return motion;
}

bool Near(QPointF a, QPointF b) {
  assert(std::isfinite(a.x()) && std::isfinite(a.y()));
  assert(std::isfinite(b.x()) && std::isfinite(b.y()));
  return std::abs(a.x() - b.x()) < 1e-6 && std::abs(a.y() - b.y()) < 1e-6;
}

}  // namespace

class WarpMotionTests final : public QObject {
  Q_OBJECT

 private slots:
  void StillWhenOffTheGrid();
  void SwaySwingsAlongItsAngle();
  void DelayStartsLate();
  void PulseBeatsTwiceThenRests();
  void BreatheSwellsAndFalls();
  void ShiverShakesEveryWay();
  void NeighboursFollowByReach();
  void WaveReachesNeighboursLate();
};

void WarpMotionTests::StillWhenOffTheGrid() {
  const PointMotion sway = Motion(WarpMotionKind::kSway);
  QVERIFY(PointMotionPushes(WarpGrid(), sway, 1.0, Frame(6)).empty());
  PointMotion off = sway;
  off.point = 9;
  QCOMPARE(PointMotionPushes(WarpGrid{2, 2}, off, 1.0, Frame(6)),
           std::vector<QPointF>(9, QPointF()));
  off = sway;
  off.kind = WarpMotionKind::kNone;
  QCOMPARE(PointMotionPushes(WarpGrid{2, 2}, off, 1.0, Frame(6)),
           std::vector<QPointF>(9, QPointF()));
}

void WarpMotionTests::SwaySwingsAlongItsAngle() {
  PointMotion sway = Motion(WarpMotionKind::kSway);
  QVERIFY(Near(MotionPush(sway, Frame(0)), QPointF()));
  QVERIFY(Near(MotionPush(sway, Frame(6)), QPointF(10, 0)));
  QVERIFY(Near(MotionPush(sway, Frame(18)), QPointF(-10, 0)));
  QVERIFY(Near(MotionPush(sway, Frame(30)), MotionPush(sway, Frame(6))));
  sway.angle = 90.0;
  QVERIFY(Near(MotionPush(sway, Frame(6)), QPointF(0, 10)));
}

void WarpMotionTests::DelayStartsLate() {
  PointMotion sway = Motion(WarpMotionKind::kSway);
  sway.delay = 0.25;
  // A quarter cycle late: at frame 6 it is where it began.
  QVERIFY(Near(MotionPush(sway, Frame(6)), QPointF()));
  QVERIFY(Near(MotionPush(sway, Frame(12)), QPointF(10, 0)));
  QVERIFY(Near(MotionPush(sway, Frame(0)), QPointF(-10, 0)));
}

void WarpMotionTests::PulseBeatsTwiceThenRests() {
  PointMotion pulse = Motion(WarpMotionKind::kPulse);
  pulse.cycle = 50;
  // The first swell tops out 8% into the cycle: frame 4 of 50.
  QVERIFY(std::abs(MotionPush(pulse, Frame(4)).x() - 10.0) < 1e-3);
  const double dub = MotionPush(pulse, Frame(14)).x();
  QVERIFY(dub > 5.0 && dub < 7.0);
  QVERIFY(std::abs(MotionPush(pulse, Frame(30)).x()) < 1e-3);
}

void WarpMotionTests::BreatheSwellsAndFalls() {
  const PointMotion breathe = Motion(WarpMotionKind::kBreathe);
  QVERIFY(Near(MotionPush(breathe, Frame(0)), QPointF()));
  QVERIFY(Near(MotionPush(breathe, Frame(6)), QPointF(5, 0)));
  QVERIFY(Near(MotionPush(breathe, Frame(12)), QPointF(10, 0)));
  QVERIFY(Near(MotionPush(breathe, Frame(18)), QPointF(5, 0)));
}

void WarpMotionTests::ShiverShakesEveryWay() {
  const PointMotion shiver = Motion(WarpMotionKind::kShiver);
  const QPointF first = MotionPush(shiver, Frame(0));
  // A fresh shake every 2 frames on a 24-frame cycle.
  QCOMPARE(MotionPush(shiver, Frame(1)), first);
  QVERIFY(MotionPush(shiver, Frame(2)) != first);
  PointMotion other = shiver;
  other.point = 3;
  QVERIFY(MotionPush(other, Frame(0)) != first);
  QVERIFY(std::abs(first.x()) <= 10.0 && std::abs(first.y()) <= 10.0);
  QVERIFY(first.y() != 0.0);
}

void WarpMotionTests::NeighboursFollowByReach() {
  const PointMotion sway = Motion(WarpMotionKind::kSway);
  const WarpGrid grid{2, 2};
  const std::vector<QPointF> alone =
      PointMotionPushes(grid, sway, 0.0, Frame(6));
  QVERIFY(Near(alone[4], QPointF(10, 0)));
  QCOMPARE(alone[3], QPointF());
  // One cell off with a reach of 2: half the push, in step.
  const std::vector<QPointF> rubber =
      PointMotionPushes(grid, sway, 2.0, Frame(6));
  QVERIFY(Near(rubber[4], QPointF(10, 0)));
  QVERIFY(Near(rubber[3], QPointF(5, 0)));
  QVERIFY(Near(rubber[5], rubber[3]));
}

void WarpMotionTests::WaveReachesNeighboursLate() {
  const PointMotion wave = Motion(WarpMotionKind::kWave);
  const WarpGrid grid{2, 2};
  const std::vector<QPointF> pushes =
      PointMotionPushes(grid, wave, 2.0, Frame(6));
  QVERIFY(Near(pushes[4], QPointF(10, 0)));
  // One cell of a reach of 2 is a quarter cycle late: still at rest.
  QVERIFY(Near(pushes[3], QPointF()));
  QVERIFY(Near(PointMotionPushes(grid, wave, 2.0, Frame(12))[3],
               QPointF(5, 0)));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::WarpMotionTests)
#include "warp_motion_tests.moc"
