#include <QLineF>
#include <QTest>

#include <cmath>

#include "anim/bone_ik.h"
#include "anim/character_pose.h"
#include "anim/vector_path.h"
#include "model/character.h"

namespace snapper {
namespace {

bool Near(QPointF a, QPointF b) { return QLineF(a, b).length() < 1e-6; }

CharacterPart Part(int id, PartKind kind, int parent) {
  CharacterPart part;
  part.id = PartId(id);
  part.name = QStringLiteral("part %1").arg(id);
  part.kind = kind;
  part.parent = PartId(parent);
  return part;
}

// Skeleton (1) with upper arm (2) from (0,0) to (100,0) and forearm (3)
// on to (200,0); Arm (4) a vector line with points 5 at (50,0) and 6 at
// (150,0); Hand (7) an image riding the forearm; Mouth switch (8) with
// Open (9) and Shut (10).
Character Arm() {
  Character character;
  CharacterPart skeleton = Part(1, PartKind::kBone, 0);
  skeleton.bones = {
      Bone{.id = BoneId(2), .name = "upper", .length = 100.0, .strength = 30.0},
      Bone{.id = BoneId(3), .name = "fore", .parent = BoneId(2),
           .base = {100, 0}, .length = 100.0, .strength = 30.0}};
  CharacterPart arm = Part(4, PartKind::kVector, 1);
  arm.drawing.points = {VectorPoint{PointId(5), {50, 0}},
                        VectorPoint{PointId(6), {150, 0}}};
  arm.drawing.curves = {{{PointId(5), PointId(6)}, false}};
  arm.drawing.shapes = {VectorShape{.curves = {0}, .has_fill = false}};
  CharacterPart hand = Part(7, PartKind::kImage, 1);
  hand.bind = BindMode::kLayer;
  hand.bound_bone = BoneId(3);
  hand.offset = {200, 0};
  CharacterPart mouth = Part(8, PartKind::kSwitch, 0);
  mouth.rest_pick = PartId(10);
  character.parts = {skeleton, arm, hand, mouth,
                     Part(9, PartKind::kVector, 8),
                     Part(10, PartKind::kVector, 8)};
  character.next_id = 11;
  return character;
}

const PosedPart* Shown(const PosedCharacter& posed, int id) {
  for (const PosedPart& part : posed.parts) {
    if (part.id == PartId(id)) {
      return &part;
    }
  }
  return nullptr;
}

}  // namespace

class CharacterPoseTests final : public QObject {
  Q_OBJECT

 private slots:
  void RestPoseKeepsTheDrawing();
  void TurningTheForearmBendsItsPointsOnly();
  void RootMovesEverything();
  void ARidingPartFollowsItsBone();
  void SwitchShowsOneChild();
  void FlexiWeightsSumToOne();
  void ActionsBlendBetweenStops();
  void SmartBoneTurnsKeepTheirOwnBone();
  void IkReachesATargetInReach();
  void IkKeepsLimits();
  void LerpHoldsThePickUntilTheNextKey();
  void CornerCurveIsStraight();
  void BouncingBonesTrailAndSettle();
};

void CharacterPoseTests::RestPoseKeepsTheDrawing() {
  const PosedCharacter posed = PoseCharacter(Arm(), CharacterPose{});
  const PosedPart* arm = Shown(posed, 4);
  QVERIFY(arm != nullptr);
  QVERIFY(Near(arm->points[0], {50, 0}));
  QVERIFY(Near(arm->points[1], {150, 0}));
  QCOMPARE(posed.bones.size(), size_t{2});
  QVERIFY(Near(posed.bones[1].tip, {200, 0}));
}

void CharacterPoseTests::TurningTheForearmBendsItsPointsOnly() {
  CharacterPose pose;
  pose.bones[BoneId(3)].turn = 90.0;
  const PosedCharacter posed = PoseCharacter(Arm(), pose);
  const PosedPart* arm = Shown(posed, 4);
  QVERIFY(Near(arm->points[0], {50, 0}));
  QVERIFY(Near(arm->points[1], {100, 50}));
  QVERIFY(Near(posed.bones[1].tip, {100, 100}));
  QVERIFY(std::abs(posed.bones[1].angle - 90.0) < 1e-6);
}

void CharacterPoseTests::RootMovesEverything() {
  CharacterPose pose;
  pose.root.offset = {10, 20};
  const PosedCharacter posed = PoseCharacter(Arm(), pose);
  QVERIFY(Near(Shown(posed, 4)->points[0], {60, 20}));
  QVERIFY(Near(posed.bones[0].base, {10, 20}));
}

void CharacterPoseTests::ARidingPartFollowsItsBone() {
  CharacterPose pose;
  pose.bones[BoneId(2)].turn = 90.0;
  const PosedCharacter posed = PoseCharacter(Arm(), pose);
  const PosedPart* hand = Shown(posed, 7);
  QVERIFY(hand != nullptr);
  QVERIFY(Near(hand->to_character.map(QPointF(0, 0)), {0, 200}));
}

void CharacterPoseTests::SwitchShowsOneChild() {
  QVERIFY(Shown(PoseCharacter(Arm(), {}), 10) != nullptr);
  QVERIFY(Shown(PoseCharacter(Arm(), {}), 9) == nullptr);
  CharacterPose pose;
  pose.parts[PartId(8)].pick = PartId(9);
  QVERIFY(Shown(PoseCharacter(Arm(), pose), 9) != nullptr);
  QVERIFY(Shown(PoseCharacter(Arm(), pose), 10) == nullptr);
}

void CharacterPoseTests::FlexiWeightsSumToOne() {
  const CharacterPart skeleton = Arm().parts[0];
  const std::vector<double> between = FlexiWeights(skeleton, {100, 5});
  QVERIFY(std::abs(between[0] + between[1] - 1.0) < 1e-9);
  QVERIFY(between[0] > 0.0 && between[1] > 0.0);
  const std::vector<double> far = FlexiWeights(skeleton, {190, 300});
  QCOMPARE(far[1], 1.0);
}

void CharacterPoseTests::ActionsBlendBetweenStops() {
  SmartAction action{BoneId(2), "lift", {}};
  ActionStop up{90.0, {}};
  up.pose.parts[PartId(4)].offset = {0, -40};
  up.pose.parts[PartId(8)].pick = PartId(9);
  action.stops = {up};
  const CharacterPose half = ActionAt(action, 45.0);
  QVERIFY(Near(half.parts.at(PartId(4)).offset, {0, -20}));
  QCOMPARE(half.parts.at(PartId(8)).pick, PartId(9));
  QVERIFY(!ActionAt(action, 30.0).parts.at(PartId(8)).pick.IsValid());
  QVERIFY(Near(ActionAt(action, 200.0).parts.at(PartId(4)).offset, {0, -40}));
}

void CharacterPoseTests::SmartBoneTurnsKeepTheirOwnBone() {
  Character character = Arm();
  ActionStop stop{90.0, {}};
  stop.pose.bones[BoneId(2)].turn = 500.0;
  stop.pose.bones[BoneId(3)].turn = 30.0;
  character.actions = {{BoneId(2), "fold", {stop}}};
  CharacterPose hand;
  hand.bones[BoneId(2)].turn = 90.0;
  const CharacterPose out = WithActions(character, hand);
  QCOMPARE(out.bones.at(BoneId(2)).turn, 90.0);
  QCOMPARE(out.bones.at(BoneId(3)).turn, 30.0);
}

void CharacterPoseTests::IkReachesATargetInReach() {
  const CharacterPose pose =
      ReachFor(Arm(), CharacterPose{}, BoneId(3), {100, 120}, 2);
  const std::vector<PosedBone> bones = PoseBones(Arm(), pose);
  QVERIFY(QLineF(bones[1].tip, {100, 120}).length() < 1.0);
}

void CharacterPoseTests::IkKeepsLimits() {
  Character character = Arm();
  character.parts[0].bones[1].has_limits = true;
  character.parts[0].bones[1].min_turn = -10.0;
  character.parts[0].bones[1].max_turn = 10.0;
  const CharacterPose pose =
      ReachFor(character, CharacterPose{}, BoneId(3), {100, 120}, 2);
  const double turn = pose.bones.at(BoneId(3)).turn;
  QVERIFY(turn >= -10.0 - 1e-9 && turn <= 10.0 + 1e-9);
}

void CharacterPoseTests::LerpHoldsThePickUntilTheNextKey() {
  CharacterPose a;
  CharacterPose b;
  b.parts[PartId(8)].pick = PartId(9);
  b.root.rotation = 10.0;
  const CharacterPose mid = Lerp(a, b, 0.9);
  QVERIFY(!mid.parts.at(PartId(8)).pick.IsValid());
  QCOMPARE(mid.root.rotation, 9.0);
  QCOMPARE(Lerp(a, b, 1.0).parts.at(PartId(8)).pick, PartId(9));
}

void CharacterPoseTests::CornerCurveIsStraight() {
  VectorDrawing drawing;
  drawing.points = {VectorPoint{PointId(1), {0, 0}, 0.0},
                    VectorPoint{PointId(2), {10, 10}, 0.0},
                    VectorPoint{PointId(3), {20, 0}, 0.0}};
  drawing.curves = {{{PointId(1), PointId(2), PointId(3)}, false}};
  const std::vector<QPointF> places = RestPlaces(drawing);
  const QPainterPath path = CurvePath(drawing, drawing.curves[0], places);
  QVERIFY(Near(path.pointAtPercent(0.25), {5, 5}));
  drawing.points[1].curvature = kSmoothCurvature;
  const QPainterPath round = CurvePath(drawing, drawing.curves[0], places);
  QVERIFY(Near(round.pointAtPercent(0.5), {10, 10}));
  const QPointF quarter = round.pointAtPercent(0.25);
  QVERIFY(quarter.y() > quarter.x() + 0.5);
}

void CharacterPoseTests::BouncingBonesTrailAndSettle() {
  Character character = Arm();
  character.parts[0].bones[1].dynamics = 0.6;
  CharacterLayer layer{"Arm", {}, false};
  CharacterPose up;
  up.bones[BoneId(2)].turn = -90.0;
  SetKey(&layer.poses, {Frame(0), CharacterPose{}, Ease::kStep});
  SetKey(&layer.poses, {Frame(10), up, Ease::kStep});
  // Right after the upper arm snaps up, the forearm trails behind it.
  const CharacterPose just = PoseAt(character, layer, Frame(11));
  QVERIFY(std::abs(just.bones.at(BoneId(3)).turn) > 5.0);
  // Long after, it has settled straight again.
  const CharacterPose later = PoseAt(character, layer, Frame(90));
  const auto settled = later.bones.find(BoneId(3));
  QVERIFY(settled == later.bones.end() || std::abs(settled->second.turn) < 1.0);
  // Without bounce nothing trails.
  character.parts[0].bones[1].dynamics = 0.0;
  QVERIFY(PoseAt(character, layer, Frame(11)).bones.count(BoneId(3)) == 0);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::CharacterPoseTests)
#include "character_pose_tests.moc"
