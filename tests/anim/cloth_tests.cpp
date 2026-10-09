#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_drag.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

constexpr int kAcross = 3;
constexpr double kGap = 10.0;

// A 20x30 piece with a 2x3 grid, 10 pixels between points. The top row
// is the body (pins); every point below it is a physics node.
Doll Shirt(const DragNode& setting) {
  Doll doll;
  doll.art.pieces = {{"shirt", {"s.png"}, 0, {0, 0}, {20, 30}}};
  doll.rig.pieces = {{"shirt", "", {10, 0}, 0, -1, {2, 3}}};
  for (int point = kAcross; point < 12; ++point) {
    DragNode node = setting;
    node.point = point;
    node.physics = true;
    doll.rig.pieces[0].drag_nodes.push_back(node);
  }
  assert(doll.rig.pieces[0].warp.PointCount() == 12);
  assert(doll.rig.pieces[0].drag_nodes.size() == 9);
  return doll;
}

DragNode Settings(double stretch, double inertia, double air, double gravity,
                  double spring) {
  DragNode node;
  node.stretch = stretch;
  node.inertia = inertia;
  node.air = air;
  node.gravity = gravity;
  node.spring = spring;
  assert(node.physics == false);
  assert(stretch >= 0.0 && stretch <= 1.0);
  return node;
}

// The doll holds still, then jumps by move at frame 2 and stays.
Layer Jump(QPointF move) {
  Layer layer;
  layer.content = DollLayer{"Shirt", {}, false};
  PiecePose moved;
  moved.offset = move;
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  assert(layer.transform.keys.size() == 2);
  assert(std::holds_alternative<DollLayer>(layer.content));
  return layer;
}

QPointF Offset(const Doll& doll, const Layer& layer, int frame, int point) {
  assert(frame >= 0);
  assert(point >= 0 && point < 12);
  const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
  return poses.at("shirt").warp.at(static_cast<size_t>(point));
}

QPointF Rest(int point) {
  assert(point >= 0 && point < 12);
  assert(kAcross == 3);
  return QPointF(point % kAcross * kGap, point / kAcross * kGap);
}

}  // namespace

class ClothTests final : public QObject {
  Q_OBJECT

 private slots:
  void StretchZeroKeepsTheLength();
  void MovingFastDownPullsTheShirtUp();
  void MoreAirSettlesFaster();
  void GravitySagsAStillDolls();
  void BounceOvershootsAndSettles();
  void PhysicsOffIsTheDragNodeAsBefore();
  void AResumedTrailGivesTheSamePoses();
};

void ClothTests::StretchZeroKeepsTheLength() {
  const Doll doll = Shirt(Settings(0.0, 0.6, 0.5, 0.3, 0.2));
  const Layer layer = Jump(QPointF(0, -60));
  double longest = 0.0;
  for (int frame = 2; frame <= 12; frame += 2) {
    const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
    const auto& warp = poses.at("shirt").warp;
    for (int point = 0; point < 12; ++point) {
      const bool has_right = point % kAcross < kAcross - 1;
      const bool has_below = point + kAcross < 12;
      for (const int next : {point + 1, point + kAcross}) {
        const bool is_edge = (next == point + 1 && has_right) ||
                             (next == point + kAcross && has_below);
        // A pin (the top row) gives by design; cloth ties do not.
        const bool is_cloth_edge = is_edge && point >= kAcross;
        if (is_cloth_edge) {
          const QPointF gap = Rest(next) + warp[static_cast<size_t>(next)] -
                              Rest(point) - warp[static_cast<size_t>(point)];
          longest = std::max(longest, std::hypot(gap.x(), gap.y()));
        }
      }
    }
  }
  assert(longest > 0.0);
  QVERIFY2(longest < kGap * 1.02, qPrintable(QString::number(longest)));
}

void ClothTests::MovingFastDownPullsTheShirtUp() {
  const Doll doll = Shirt(Settings(0.0, 0.6, 0.5, 0.0, 0.0));
  const Layer layer = Jump(QPointF(0, 40));
  const QPointF at = Offset(doll, layer, 2, 11);
  QVERIFY2(at.y() < -10.0, qPrintable(QString::number(at.y())));
  // The top row is the body: it does not move in the drawing.
  QCOMPARE(Offset(doll, layer, 2, 1), QPointF());
}

void ClothTests::MoreAirSettlesFaster() {
  const Layer layer = Jump(QPointF(0, 40));
  double left[2] = {0.0, 0.0};
  const double airs[2] = {0.1, 0.9};
  for (int i = 0; i < 2; ++i) {
    const Doll doll = Shirt(Settings(1.0, 1.0, airs[i], 0.0, 1.0));
    for (int frame = 30; frame <= 40; frame += 2) {
      left[i] = std::max(left[i], std::abs(Offset(doll, layer, frame, 11).y()));
    }
  }
  QVERIFY2(left[1] < left[0], qPrintable(QString::number(left[0])));
  QVERIFY(left[1] < 1.0);
}

void ClothTests::GravitySagsAStillDolls() {
  Layer still;
  still.content = DollLayer{"Shirt", {}, false};
  const Doll doll = Shirt(Settings(0.8, 0.6, 0.5, 1.0, 0.0));
  QCOMPARE(Offset(doll, still, 0, 11), QPointF());
  const QPointF sagged = Offset(doll, still, 10, 11);
  QVERIFY2(sagged.y() > 0.5, qPrintable(QString::number(sagged.y())));
  const Doll firm = Shirt(Settings(0.0, 0.6, 0.5, 1.0, 0.0));
  QVERIFY(Offset(firm, still, 10, 11).y() < sagged.y());
}

void ClothTests::BounceOvershootsAndSettles() {
  const Doll doll = Shirt(Settings(1.0, 0.9, 0.1, 0.0, 1.0));
  const Layer layer = Jump(QPointF(6, 0));
  double first = 0.0;
  double highest = -1e9;
  for (int frame = 2; frame <= 40; frame += 2) {
    const double x = Offset(doll, layer, frame, 4).x();
    first = frame == 2 ? x : first;
    highest = std::max(highest, x);
  }
  QVERIFY2(first < -1.0, qPrintable(QString::number(first)));
  QVERIFY2(highest > 0.3, qPrintable(QString::number(highest)));
  QVERIFY(std::abs(Offset(doll, layer, 300, 4).x()) < 0.1);
}

void ClothTests::PhysicsOffIsTheDragNodeAsBefore() {
  Doll plain = Shirt(Settings(0.0, 0.6, 0.5, 0.3, 0.2));
  for (DragNode& node : plain.rig.pieces[0].drag_nodes) {
    node.physics = false;
  }
  Doll tuned = plain;
  for (DragNode& node : tuned.rig.pieces[0].drag_nodes) {
    node.stretch = 0.9;
    node.gravity = 1.0;
    node.air = 0.1;
  }
  const Layer layer = Jump(QPointF(10, 5));
  for (int frame : {0, 2, 6, 20}) {
    QCOMPARE(DraggedPoses(plain, layer, Frame(frame)),
             DraggedPoses(tuned, layer, Frame(frame)));
  }
  Doll cloth = plain;
  for (DragNode& node : cloth.rig.pieces[0].drag_nodes) {
    node.physics = true;
  }
  QVERIFY(DraggedPoses(cloth, layer, Frame(6)) !=
          DraggedPoses(plain, layer, Frame(6)));
}

void ClothTests::AResumedTrailGivesTheSamePoses() {
  const Doll doll = Shirt(Settings(0.3, 0.6, 0.5, 0.3, 0.2));
  const Layer layer = Jump(QPointF(8, 20));
  DragTrail trail;
  for (int frame : {0, 4, 5, 30, 30, 3, 80, 12}) {
    QCOMPARE(DraggedPoses(doll, layer, Frame(frame), {}, &trail),
             DraggedPoses(doll, layer, Frame(frame)));
  }
  QVERIFY(trail.frames().size() > 50);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ClothTests)
#include "cloth_tests.moc"
