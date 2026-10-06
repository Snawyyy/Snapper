#include <QTest>

#include <cmath>

#include "anim/doll_drag.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

// A 20x20 piece with a 2x2 warp grid, its middle point a drag node.
Doll Blob(double bounce) {
  Doll doll;
  doll.art.pieces = {{"blob", {"b.png"}, 0, {0, 0}, {20, 20}}};
  doll.rig.pieces = {{"blob", "", {10, 10}, 0, -1, {2, 2}}};
  doll.rig.pieces[0].drag_nodes = {{4, 0.5, bounce}};
  return doll;
}

// A doll layer that jumps 10 pixels right at frame 2 and stays there.
Layer Jumping() {
  Layer layer;
  layer.content = DollLayer{"Blob", {}, false};
  PiecePose moved;
  moved.offset = QPointF(10, 0);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  return layer;
}

double Trail(const Doll& doll, const Layer& layer, int frame,
             size_t point = 4) {
  const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
  return poses.at("blob").warp.at(point).x();
}

}  // namespace

class DragTests final : public QObject {
  Q_OBJECT

 private slots:
  void StillDollsDontDrag();
  void ANodeTrailsThenSettles();
  void BounceOvershoots();
  void ReachSpreadsTheTrail();
  void EasesDragEveryFrame();
};

void DragTests::StillDollsDontDrag() {
  Doll doll = Blob(0.5);
  Layer layer;
  layer.content = DollLayer{"Blob", {}, false};
  for (int frame : {0, 5, 30}) {
    const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
    QVERIFY(std::abs(poses.at("blob").warp.at(4).x()) < 1e-9);
  }
  doll.rig.pieces[0].drag_nodes.clear();
  QVERIFY(DraggedPoses(doll, layer, Frame(3)).empty());
}

void DragTests::ANodeTrailsThenSettles() {
  const Doll doll = Blob(0.0);
  const Layer layer = Jumping();
  QCOMPARE(Trail(doll, layer, 1), 0.0);
  // Jumped right: the node is left behind, then catches up.
  QVERIFY(Trail(doll, layer, 2) < -3.0);
  QVERIFY(Trail(doll, layer, 4) > Trail(doll, layer, 2));
  QVERIFY(std::abs(Trail(doll, layer, 60)) < 0.05);
  // Keyed on 2s, it drags on 2s: held frames hold the trail too.
  QCOMPARE(Trail(doll, layer, 3), Trail(doll, layer, 2));
  QVERIFY(Trail(doll, layer, 4) != Trail(doll, layer, 3));
  QCOMPARE(Trail(doll, layer, 9), Trail(doll, layer, 8));
  // The same frame always comes out the same.
  QCOMPARE(Trail(doll, layer, 7), Trail(doll, layer, 7));
}

void DragTests::BounceOvershoots() {
  const Layer layer = Jumping();
  double most = 0.0;
  for (int frame = 2; frame < 30; ++frame) {
    most = std::max(most, Trail(Blob(1.0), layer, frame));
  }
  // Bouncy: it swings past where it should be.
  QVERIFY(most > 1.0);
  double calm = 0.0;
  for (int frame = 2; frame < 30; ++frame) {
    calm = std::max(calm, Trail(Blob(0.0), layer, frame));
  }
  QVERIFY(calm < 0.5);
}

void DragTests::ReachSpreadsTheTrail() {
  Doll doll = Blob(0.0);
  const Layer layer = Jumping();
  QCOMPARE(Trail(doll, layer, 2, 1), 0.0);
  doll.rig.pieces[0].warp_reach = std::vector<double>(9, 0.0);
  doll.rig.pieces[0].warp_reach[4] = 2.0;
  // One cell off, half the pull.
  QVERIFY(std::abs(Trail(doll, layer, 2, 1) - Trail(doll, layer, 2) / 2) <
          1e-9);
}

void DragTests::EasesDragEveryFrame() {
  const Doll doll = Blob(0.0);
  Layer layer = Jumping();
  // Sliding from 2 to 12: every frame between is a beat.
  PiecePose far;
  far.offset = QPointF(40, 0);
  layer.transform.keys[1].ease = Ease::kLinear;
  SetKey(&layer.transform, {Frame(12), far, Ease::kStep});
  QVERIFY(Trail(doll, layer, 5) != Trail(doll, layer, 4));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::DragTests)
#include "drag_tests.moc"
