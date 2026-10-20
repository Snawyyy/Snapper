#include <QTest>

#include <cassert>

#include "io/character_json.h"
#include "model/character.h"

namespace snapper {
namespace {

// A bone part holding an arm bone and a vector part with a triangle.
Character Arm() {
  Character character;
  character.name = "Bo";
  CharacterPart skeleton;
  skeleton.id = PartId(1);
  skeleton.name = "Bones";
  skeleton.kind = PartKind::kBone;
  skeleton.bones = {Bone{.id = BoneId(2), .name = "arm", .length = 80.0}};
  CharacterPart body;
  body.id = PartId(3);
  body.name = "Body";
  body.kind = PartKind::kVector;
  body.parent = PartId(1);
  body.drawing.points = {VectorPoint{PointId(4), {0, 0}},
                         VectorPoint{PointId(5), {50, 0}, 0.0},
                         VectorPoint{PointId(6), {0, 50}, kSmoothCurvature, 0.5}};
  body.drawing.curves = {{{PointId(4), PointId(5), PointId(6)}, true}};
  body.drawing.shapes = {
      VectorShape{.curves = {0}, .fill = Qt::red, .brush = LineBrush::kInk}};
  body.point_bones[PointId(5)] = BoneId(2);
  character.parts = {skeleton, body};
  character.actions = {
      {BoneId(2), "wave", {{45.0, {.bones = {{BoneId(2), {.turn = 10}}}}}}}};
  character.next_id = 7;
  assert(IsSound(character));
  return character;
}

}  // namespace

class CharacterJsonTests final : public QObject {
  Q_OBJECT

 private slots:
  void CharacterRoundTrips();
  void BrokenTreeLoadsEmpty();
  void PoseRoundTripsAndSkipsRest();
  void MissingPoseFallsBack();
};

void CharacterJsonTests::CharacterRoundTrips() {
  const Character character = Arm();
  JsonIssues issues;
  const Character back =
      CharacterFromJson(CharacterToJson(character), &issues);
  QVERIFY(!issues.HasIssue());
  QCOMPARE(back, character);
}

void CharacterJsonTests::BrokenTreeLoadsEmpty() {
  QJsonObject json = CharacterToJson(Arm());
  QJsonArray parts = json.value("parts").toArray();
  QJsonObject child = parts[1].toObject();
  child.insert("id", 1);
  parts[1] = child;
  json.insert("parts", parts);
  JsonIssues issues;
  const Character back = CharacterFromJson(json, &issues);
  QVERIFY(issues.HasIssue());
  QVERIFY(back.parts.empty());
  QCOMPARE(back.name, QStringLiteral("Bo"));
}

void CharacterJsonTests::PoseRoundTripsAndSkipsRest() {
  CharacterPose pose;
  pose.root.offset = {10, -4};
  pose.parts[PartId(3)].points[PointId(5)] = {2, 3};
  pose.parts[PartId(1)] = PartPose{};
  pose.bones[BoneId(2)].turn = 30.0;
  const QJsonValue json = ValueToJson(pose);
  QVERIFY(!json.toObject().value("parts").toObject().contains("1"));
  JsonIssues issues;
  const CharacterPose back = ValueFromJson(json, CharacterPose{}, &issues);
  QVERIFY(!issues.HasIssue());
  pose.parts.erase(PartId(1));
  QCOMPARE(back, pose);
}

void CharacterJsonTests::MissingPoseFallsBack() {
  CharacterPose fallback;
  fallback.root.rotation = 5.0;
  JsonIssues issues;
  QCOMPARE(ValueFromJson(QJsonValue(), fallback, &issues), fallback);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::CharacterJsonTests)
#include "character_json_tests.moc"
