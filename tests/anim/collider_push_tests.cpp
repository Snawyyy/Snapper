#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_drag.h"
#include "anim/grid_push.h"
#include "leg_skirt.h"

namespace snapper {
namespace {

using leg_skirt::CameIn;
using leg_skirt::LegAndSkirt;
using leg_skirt::Shown;

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// How far the furthest skirt point is from its drawn place.
double Furthest(const PoseMap& poses) {
  assert(poses.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(leg_skirt::kPoints == 20);
  const std::vector<QPointF> now = Shown(poses);
  const std::vector<QPointF> rest = Shown(PoseMap());
  double furthest = 0.0;
  for (size_t i = 0; i < now.size(); ++i) {
    furthest = std::max(furthest, Length(now[i] - rest[i]));
  }
  return furthest;
}

// The thigh kicks out to 60 degrees one way, back, and the other way.
Layer KickBothWays() {
  assert(leg_skirt::kHem == 15);
  assert(leg_skirt::kAcross == 5);
  return leg_skirt::Kicking(
      {{0, 0.0}, {6, 60.0}, {12, 0.0}, {18, -60.0}, {24, 0.0}});
}

}  // namespace

class ColliderPushTests final : public QObject {
  Q_OBJECT

 private slots:
  void AColliderAtRestChangesNothing();
  void APlainSkirtNeverLetsTheThighIn();
  void TheSkirtIsFreeToMoveAway();
  void AnUntickedPieceIsNotPushed();
  void APushBendsTheNeighboursLess();
  void PhysicsAndDragDotsArePushedToo();
  void ScrubbingGivesWhatPlayingGives();
};

void ColliderPushTests::AColliderAtRestChangesNothing() {
  const Doll with = LegAndSkirt();
  Doll without = with;
  without.rig.pieces[0].collider = Collider();
  // The thigh is drawn under the skirt: as drawn, nothing is pushed.
  for (int frame : {0, 5, 30}) {
    QCOMPARE(DraggedPoses(with, leg_skirt::Still(), Frame(frame)),
             DraggedPoses(without, leg_skirt::Still(), Frame(frame)));
  }
  // The same with the whole doll moving: pushes follow the doll.
  Layer moving = leg_skirt::Still();
  PiecePose far;
  far.offset = QPointF(300, -40);
  far.rotation = 70.0;
  SetKey(&moving.transform, {Frame(0), PiecePose(), Ease::kLinear});
  SetKey(&moving.transform, {Frame(8), far, Ease::kLinear});
  QCOMPARE(Furthest(DraggedPoses(with, moving, Frame(4))), 0.0);
}

void ColliderPushTests::APlainSkirtNeverLetsTheThighIn() {
  const Doll doll = LegAndSkirt();
  const Layer layer = KickBothWays();
  double pushed = 0.0;
  for (int frame = 0; frame <= 26; ++frame) {
    const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
    QVERIFY2(CameIn(doll, poses, 0.5) == 0,
             qPrintable(QString("frame %1: %2 spots in")
                            .arg(frame)
                            .arg(CameIn(doll, poses, 0.5))));
    pushed = std::max(pushed, Furthest(poses));
  }
  // It did have to push, a good way.
  QVERIFY2(pushed > 10.0, qPrintable(QString::number(pushed)));
  // Back down, the skirt is as drawn again.
  QCOMPARE(Furthest(DraggedPoses(doll, layer, Frame(24))), 0.0);
}

void ColliderPushTests::TheSkirtIsFreeToMoveAway() {
  const Doll doll = LegAndSkirt();
  Layer layer = leg_skirt::Kicking({{0, 0.0}, {6, 60.0}});
  auto& skirt = std::get<DollLayer>(layer.content).pieces["skirt"];
  PiecePose up;
  up.offset = QPointF(0, -120);
  SetKey(&skirt, {Frame(6), PiecePose(), Ease::kLinear});
  SetKey(&skirt, {Frame(12), up, Ease::kStep});
  QVERIFY(Furthest(DraggedPoses(doll, layer, Frame(6))) > 5.0);
  // Flown up off the thigh: nothing holds it or pulls it back down.
  QCOMPARE(Furthest(DraggedPoses(doll, layer, Frame(12))), 0.0);
}

void ColliderPushTests::AnUntickedPieceIsNotPushed() {
  const Layer layer = KickBothWays();
  Doll spared = LegAndSkirt();
  spared.rig.pieces[0].collider.spared = {"skirt"};
  Doll behind = LegAndSkirt();
  behind.rig.pieces[1].order = -1;
  for (int frame : {3, 6, 18}) {
    QCOMPARE(Furthest(DraggedPoses(spared, layer, Frame(frame))), 0.0);
  }
  // A piece drawn behind the collider is pushed all the same: colliders
  // work both ways, so two hands push each other apart.
  QVERIFY(Furthest(DraggedPoses(behind, layer, Frame(6))) > 5.0);
}

void ColliderPushTests::APushBendsTheNeighboursLess() {
  const Doll doll = LegAndSkirt();
  const Layer layer = KickBothWays();
  const std::vector<QPointF> rest = Shown(PoseMap());
  const std::vector<QPointF> now =
      Shown(DraggedPoses(doll, layer, Frame(6)));
  size_t most = 0;
  for (size_t i = 0; i < now.size(); ++i) {
    const bool is_more =
        Length(now[i] - rest[i]) > Length(now[most] - rest[most]);
    most = is_more ? i : most;
  }
  const double push = Length(now[most] - rest[most]);
  QVERIFY(push > 5.0);
  // The point above it follows some of the way: the drawing bends
  // instead of kinking at one point.
  const size_t above = most - static_cast<size_t>(leg_skirt::kAcross);
  const double follow = Length(now[above] - rest[above]);
  QVERIFY2(follow > push * 0.1 && follow < push,
           qPrintable(QString("%1 of %2").arg(follow).arg(push)));
}

void ColliderPushTests::PhysicsAndDragDotsArePushedToo() {
  const Layer layer = KickBothWays();
  Doll cloth = LegAndSkirt();
  leg_skirt::MakeCloth(&cloth, 2);
  Doll dragged = LegAndSkirt();
  for (int point = leg_skirt::kHem; point < leg_skirt::kPoints; ++point) {
    dragged.rig.pieces[1].drag_nodes.push_back({point, 0.6, 0.6});
  }
  QCOMPARE(cloth.rig.pieces[1].drag_nodes.size(), size_t{10});
  for (int frame = 0; frame <= 40; ++frame) {
    for (const Doll* doll : {&cloth, &dragged}) {
      const PoseMap poses = DraggedPoses(*doll, layer, Frame(frame));
      const std::vector<QPointF> points = Shown(poses);
      for (int hem = leg_skirt::kHem; hem < leg_skirt::kPoints; ++hem) {
        const bool was_in = leg_skirt::IsInThigh(
            *doll, PoseMap(), Shown(PoseMap())[static_cast<size_t>(hem)],
            0.0);
        const bool is_in = leg_skirt::IsInThigh(
            *doll, poses, points[static_cast<size_t>(hem)], 1.0);
        QVERIFY2(was_in || !is_in,
                 qPrintable(QString("frame %1 dot %2").arg(frame).arg(hem)));
      }
    }
  }
}

void ColliderPushTests::ScrubbingGivesWhatPlayingGives() {
  Doll doll = LegAndSkirt();
  leg_skirt::MakeCloth(&doll, 2);
  const Layer layer = KickBothWays();
  DragTrail trail;
  for (int frame : {0, 3, 7, 30, 30, 5, 80, 12, 6}) {
    QCOMPARE(DraggedPoses(doll, layer, Frame(frame), {}, &trail),
             DraggedPoses(doll, layer, Frame(frame)));
  }
  QVERIFY(trail.frames().size() > 50);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ColliderPushTests)
#include "collider_push_tests.moc"
