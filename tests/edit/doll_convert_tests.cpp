#include <QLineF>
#include <QTest>

#include <cassert>

#include "anim/character_pose.h"
#include "edit/doll_convert.h"

namespace snapper {
namespace {

// A body with an arm hanging from its right side; the arm has two
// drawings.
Doll Arm() {
  Doll doll;
  doll.name = "Bob";
  doll.folder = "/dolls/Bob.doll";
  doll.art.canvas = QSize(400, 400);
  doll.art.pieces = {
      ArtPiece{.name = "body", .drawings = {"body.png"}, .position = {0, 0},
               .size = {100, 100}},
      ArtPiece{.name = "arm", .drawings = {"open.png", "fist.png"},
               .position = {100, 0}, .size = {100, 20}}};
  RigPiece body;
  body.name = "body";
  body.pivot = {50, 50};
  RigPiece arm;
  arm.name = "arm";
  arm.parent = "body";
  arm.pivot = {0, 10};
  arm.order = 1;
  doll.rig.pieces = {body, arm};
  SmartDial wave;
  wave.name = "wave";
  wave.pieces = {"arm"};
  DialStop rest;
  rest.name = "rest";
  DialStop up;
  up.name = "up";
  up.at = 1.0;
  up.poses["arm"].rotation = -90.0;
  up.drawings["arm"] = "arm 2";
  wave.stops = {rest, up};
  doll.rig.dials = {wave};
  return doll;
}

}  // namespace

class DollConvertTests final : public QObject {
  Q_OBJECT

 private slots:
  void PiecesBecomeBonesAndPictures();
  void RestLooksAsDrawn();
  void KeysTurnBonesAndPickDrawings();
  void DialsBecomeSmartBones();
  void ProjectsSwapDollLayersForCharacters();
};

void DollConvertTests::PiecesBecomeBonesAndPictures() {
  DollParts parts;
  const Character character = CharacterFromDoll(Arm(), &parts);
  QVERIFY(IsSound(character));
  QCOMPARE(FindPart(character, parts.parts.at("body"))->kind, PartKind::kImage);
  QCOMPARE(FindPart(character, parts.parts.at("arm"))->kind, PartKind::kSwitch);
  QCOMPARE(parts.drawings.at("arm").size(), size_t{2});
  QCOMPARE(FindBone(character, parts.bones.at("arm"))->parent,
           parts.bones.at("body"));
}

void DollConvertTests::RestLooksAsDrawn() {
  DollParts parts;
  const Character character = CharacterFromDoll(Arm(), &parts);
  const PosedCharacter posed = PoseCharacter(character, CharacterPose{});
  for (const PosedPart& part : posed.parts) {
    const CharacterPart* rest = FindPart(character, part.id);
    if (rest->name == "body") {
      QVERIFY(QLineF(part.to_character.map(QPointF()), {0, 0}).length() < 1e-6);
    } else {
      QVERIFY(QLineF(part.to_character.map(QPointF()), {100, 0}).length() < 1e-6);
    }
  }
}

void DollConvertTests::KeysTurnBonesAndPickDrawings() {
  const Doll doll = Arm();
  DollParts parts;
  const Character character = CharacterFromDoll(doll, &parts);
  DollLayer layer{"Bob", {}, true};
  PiecePose bent;
  bent.rotation = 90.0;
  bent.drawing = 1;
  layer.pieces["arm"].keys = {Key<PiecePose>{Frame(4), bent, Ease::kLinear}};
  const CharacterLayer converted = CharacterLayerFromDoll(doll, layer, parts);
  QVERIFY(converted.is_flipped);
  QCOMPARE(converted.poses.keys.size(), size_t{1});
  QCOMPARE(converted.poses.keys[0].ease, Ease::kLinear);
  const CharacterPose& pose = converted.poses.keys[0].value;
  QCOMPARE(pose.bones.at(parts.bones.at("arm")).turn, 90.0);
  QCOMPARE(pose.parts.at(parts.parts.at("arm")).pick,
           parts.drawings.at("arm")[1]);
  const PosedCharacter posed = PoseCharacter(character, pose);
  for (const PosedPart& part : posed.parts) {
    if (part.id == parts.drawings.at("arm")[1]) {
      QVERIFY(QLineF(part.to_character.map(QPointF(0, 10)), {100, 10}).length() <
              1e-6);
      QVERIFY(QLineF(part.to_character.map(QPointF(10, 10)), {100, 20}).length() <
              1e-6);
    }
  }
}

void DollConvertTests::DialsBecomeSmartBones() {
  DollParts parts;
  const Character character = CharacterFromDoll(Arm(), &parts);
  const BoneId smart = parts.dials.at("wave");
  QVERIFY(FindBone(character, smart)->is_smart);
  const SmartAction* action = FindAction(character, smart);
  QVERIFY(action != nullptr);
  QCOMPARE(action->stops.size(), size_t{1});
  QCOMPARE(action->stops[0].turn, 180.0);
  QCOMPARE(action->stops[0].pose.bones.at(parts.bones.at("arm")).turn, -90.0);
}

void DollConvertTests::ProjectsSwapDollLayersForCharacters() {
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(Arm());
  Shot shot;
  shot.id = ShotId(1);
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Bob", {}, false};
  shot.layers.push_back(layer);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  const Project converted = WithDollsAsCharacters(project);
  QVERIFY(converted.dolls.empty());
  QVERIFY(FindCharacter(converted, "Bob") != nullptr);
  QVERIFY(std::holds_alternative<CharacterLayer>(
      converted.shots[0]->layers[0].content));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::DollConvertTests)
#include "doll_convert_tests.moc"
