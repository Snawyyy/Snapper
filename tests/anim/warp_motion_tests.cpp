#include <QTest>

#include "anim/warp_motion.h"

namespace snapper {

class WarpMotionTests final : public QObject {
  Q_OBJECT

 private slots:
  void StillWhenOff();
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

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::WarpMotionTests)
#include "warp_motion_tests.moc"
