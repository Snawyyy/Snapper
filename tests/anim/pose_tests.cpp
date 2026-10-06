#include <QLineF>
#include <QTest>

#include "anim/doll_pose.h"
#include "anim/ik.h"
#include "anim/mirror.h"
#include "anim/warp.h"

namespace snapper {
namespace {

// A body with an arm hanging off its right side.
Doll BodyAndArm() {
  Doll doll;
  doll.folder = "/d";
  doll.art.pieces = {{"body", {"b.png"}, 0, {0, 0}, {10, 10}},
                     {"arm", {"a.png"}, 0, {10, 0}, {10, 2}}};
  doll.rig.pieces = {{"body", "", {5, 5}, 0, -1, {}},
                     {"arm", "body", {0, 1}, 1, -1, {}}};
  return doll;
}

// Two 10-pixel bones along x, tip at (20, 1).
Doll TwoBones() {
  Doll doll;
  doll.art.pieces = {{"up", {"u.png"}, 0, {0, 0}, {10, 2}},
                     {"low", {"l.png"}, 0, {10, 0}, {10, 2}}};
  doll.rig.pieces = {{"up", "", {0, 1}, 0, -1, {}},
                     {"low", "up", {0, 1}, 1, -1, {}}};
  doll.rig.chains = {{"arm", "up", "low", {10, 1}, true}};
  return doll;
}

QPointF TipAfter(const Doll& doll, const IkSolution& solution) {
  PoseMap poses;
  poses["up"].rotation = solution.upper_rotation;
  poses["low"].rotation = solution.lower_rotation;
  return PieceTransforms(doll, poses).at("low").map(QPointF(10, 1));
}

bool IsNear(QPointF a, QPointF b) { return QLineF(a, b).length() < 1e-6; }

}  // namespace

class PoseTests final : public QObject {
  Q_OBJECT

 private slots:
  void RestPoseKeepsTheKritaLayout();
  void ChildrenFollowTheirParent();
  void PiecesDrawBackToFront();
  void ChildrenOfMovedPartsStayInPlace();
  void PoseMatrixTurnsAroundTheCenter();
  void IkReachesATarget();
  void IkBendsEitherWay();
  void IkPointsAtTargetsOutOfReach();
  void IkRefusesBrokenChains();
  void WarpPushesGridPoints();
  void MirrorSwapsSidesAndFlipsTurns();
};

void PoseTests::RestPoseKeepsTheKritaLayout() {
  const auto transforms = PieceTransforms(BodyAndArm(), PoseMap());
  QCOMPARE(transforms.at("arm").map(QPointF(0, 1)), QPointF(10, 1));
  QCOMPARE(transforms.at("body").map(QPointF(0, 0)), QPointF(0, 0));
}

void PoseTests::ChildrenFollowTheirParent() {
  PoseMap poses;
  poses["body"].rotation = 90.0;
  const auto transforms = PieceTransforms(BodyAndArm(), poses);
  QVERIFY(IsNear(transforms.at("arm").map(QPointF(0, 1)), QPointF(9, 10)));
  poses["arm"].offset = QPointF(3, 0);
  poses["body"].rotation = 0.0;
  QVERIFY(IsNear(PieceTransforms(BodyAndArm(), poses)
                     .at("arm").map(QPointF(0, 1)),
                 QPointF(13, 1)));
}

void PoseTests::ChildrenOfMovedPartsStayInPlace() {
  Doll doll = BodyAndArm();
  doll.art.pieces[0].position = QPointF(100, 50);
  doll.art.pieces[1].position = QPointF(110, 50);
  doll.rig.pieces[0].pivot = QPointF(5, 5);
  const auto rest = PieceTransforms(doll, PoseMap());
  QVERIFY(IsNear(rest.at("arm").map(QPointF(0, 1)), QPointF(110, 51)));
  PoseMap poses;
  poses["body"].rotation = 90.0;
  // The arm's joint swings around the body's at (105, 55).
  QVERIFY(IsNear(PieceTransforms(doll, poses).at("arm").map(QPointF(0, 1)),
                 QPointF(109, 60)));
  doll.rig.pieces[1].rest_rotation = 90.0;
  QVERIFY(IsNear(PieceTransforms(doll, PoseMap()).at("arm")
                     .map(QPointF(1, 1)),
                 QPointF(110, 52)));
}

void PoseTests::PiecesDrawBackToFront() {
  Doll doll = BodyAndArm();
  doll.rig.pieces[0].order = 5;
  PoseMap poses;
  poses["arm"].drawing = 0;
  poses["arm"].opacity = 0.5;
  const auto placed = PlaceDoll(doll, poses);
  QCOMPARE(placed.size(), size_t{2});
  QCOMPARE(placed[0].name, QString("arm"));
  QCOMPARE(placed[0].drawing, QString("/d/a.png"));
  QCOMPARE(placed[0].opacity, 0.5);
  QVERIFY(placed[0].warp_points.empty());
}

void PoseTests::PoseMatrixTurnsAroundTheCenter() {
  PiecePose pose;
  pose.rotation = 180.0;
  const QTransform turn = PoseMatrix(pose, QPointF(5, 5));
  QVERIFY(IsNear(turn.map(QPointF(5, 5)), QPointF(5, 5)));
  QVERIFY(IsNear(turn.map(QPointF(6, 5)), QPointF(4, 5)));
  pose = PiecePose();
  pose.scale_x = 2.0;
  QVERIFY(IsNear(PoseMatrix(pose, QPointF(5, 5)).map(QPointF(6, 5)),
                 QPointF(7, 5)));
}

void PoseTests::IkReachesATarget() {
  const Doll doll = TwoBones();
  const auto solution =
      SolveIk(doll, PoseMap(), doll.rig.chains[0], QPointF(10, 11));
  QVERIFY(solution.has_value());
  QVERIFY(solution->is_reached);
  QVERIFY(IsNear(TipAfter(doll, *solution), QPointF(10, 11)));
}

void PoseTests::IkBendsEitherWay() {
  Doll doll = TwoBones();
  const QPointF target(14, 1);
  const auto one = SolveIk(doll, PoseMap(), doll.rig.chains[0], target);
  doll.rig.chains[0].bends_clockwise = false;
  const auto other = SolveIk(doll, PoseMap(), doll.rig.chains[0], target);
  QVERIFY(IsNear(TipAfter(doll, *one), target));
  QVERIFY(IsNear(TipAfter(doll, *other), target));
  QVERIFY(one->upper_rotation * other->upper_rotation < 0.0);
}

void PoseTests::IkPointsAtTargetsOutOfReach() {
  const Doll doll = TwoBones();
  const auto solution =
      SolveIk(doll, PoseMap(), doll.rig.chains[0], QPointF(0, 101));
  QVERIFY(!solution->is_reached);
  const QPointF tip = TipAfter(doll, *solution);
  QVERIFY(std::abs(tip.x()) < 1e-3);
  QVERIFY(std::abs(tip.y() - 21.0) < 1e-3);
}

void PoseTests::IkRefusesBrokenChains() {
  Doll doll = TwoBones();
  IkChain broken = doll.rig.chains[0];
  broken.lower = "ghost";
  QVERIFY(!SolveIk(doll, PoseMap(), broken, QPointF(1, 1)).has_value());
  broken = doll.rig.chains[0];
  broken.tip = QPointF(0, 1);
  QVERIFY(!SolveIk(doll, PoseMap(), broken, QPointF(1, 1)).has_value());
}

void PoseTests::WarpPushesGridPoints() {
  const WarpGrid grid{2, 1};
  const auto rest = RestPoints(QSize(10, 4), grid);
  QCOMPARE(rest.size(), size_t{6});
  QCOMPARE(rest[4], QPointF(5, 4));
  std::vector<QPointF> offsets(6);
  offsets[1] = QPointF(0, -2);
  QCOMPARE(WarpedPoints(QSize(10, 4), grid, offsets)[1], QPointF(5, -2));
  QCOMPARE(WarpedPoints(QSize(10, 4), grid, {QPointF(1, 1)}), rest);
  QCOMPARE(NearestPoint(rest, QPointF(9, 3), 2.0), 5);
  QCOMPARE(NearestPoint(rest, QPointF(7, 2), 1.0), -1);
  QVERIFY(RestPoints(QSize(10, 4), WarpGrid()).empty());
  // Rubber: a point always takes the whole pull; others fade with
  // distance in cells and stop at the reach.
  QCOMPARE(PullWeight(grid, 1, 1, 0.0), 1.0);
  QCOMPARE(PullWeight(grid, 1, 0, 0.0), 0.0);
  QCOMPARE(PullWeight(grid, 1, 0, 2.0), 0.5);
  QCOMPARE(PullWeight(grid, 1, 5, 1.0), 0.0);
  QVERIFY(PullWeight(grid, 0, 4, 2.0) < PullWeight(grid, 0, 1, 2.0));
  QCOMPARE(MirroredPoint(grid, 0), 2);
  QCOMPARE(MirroredPoint(grid, 4), 4);
}

void PoseTests::MirrorSwapsSidesAndFlipsTurns() {
  Doll doll;
  doll.rig.pieces = {{"arm_l", "", {}, 0, -1, {}},
                     {"arm_r", "", {}, 0, -1, {}},
                     {"head", "", {}, 0, -1, {1, 1}}};
  PoseMap poses;
  poses["arm_l"].rotation = 30.0;
  poses["head"].offset = QPointF(4, 2);
  poses["head"].warp = {QPointF(1, 0), QPointF(0, 3), QPointF(), QPointF()};
  const PoseMap mirrored = MirrorPoses(doll, poses);
  QCOMPARE(mirrored.at("arm_r").rotation, -30.0);
  QCOMPARE(mirrored.at("arm_l").rotation, 0.0);
  QCOMPARE(mirrored.at("head").offset, QPointF(-4, 2));
  QCOMPARE(mirrored.at("head").warp[0], QPointF(0, 3));
  QCOMPARE(mirrored.at("head").warp[1], QPointF(-1, 0));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::PoseTests)
#include "pose_tests.moc"
