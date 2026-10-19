#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "edit/character_edits.h"
#include "edit/character_manager.h"
#include "edit/character_pose_edits.h"
#include "edit/character_pose_manager.h"
#include "edit/history_manager.h"
#include "edit/stage_manager.h"
#include "edit/vector_edits.h"

namespace snapper {
namespace {

PartId DrawingPart(const Character& character) {
  const CharacterPart* drawing = FindPart(character, QStringLiteral("Drawing"));
  assert(drawing != nullptr);
  return drawing->id;
}

Project WithOneShot() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(48);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  project.next_shot_id = 2;
  return project;
}

}  // namespace

class CharacterEditTests final : public QObject {
  Q_OBJECT

 private slots:
  void NewCharacterHasBonesHoldingADrawing();
  void CurvesWeldToNearbyEnds();
  void ExtendingAnEndGrowsItsCurve();
  void DeletingPointsTidiesCurvesAndShapes();
  void WeldingTheEndsClosesALoop();
  void ShapesComeFromWholeLines();
  void PartsMoveWithWhatTheyHold();
  void DuplicatesTakeNewIds();
  void RemovingABoneRehangsItsChildren();
  void BoneParentsNeverLoop();
  void ActionStopsSortAndSkipRest();
  void ManagerEditsAreNamedUndoSteps();
  void PosingKeysTheWholePose();
  void TrimmedDropsRest();
};

void CharacterEditTests::NewCharacterHasBonesHoldingADrawing() {
  const Character character = NewCharacter("Bo");
  QCOMPARE(character.parts.size(), size_t{2});
  QCOMPARE(character.parts[0].kind, PartKind::kBone);
  QCOMPARE(character.parts[1].parent, character.parts[0].id);
  QVERIFY(IsSound(character));
}

void CharacterEditTests::CurvesWeldToNearbyEnds() {
  VectorDrawing drawing;
  int next = 1;
  const std::vector<QPointF> first = {{0, 0}, {100, 0}};
  QVERIFY(AddCurve(&drawing, &next, first, false, 0.0, 8.0).has_value());
  const std::vector<QPointF> second = {{103, 2}, {100, 100}};
  const auto ids = AddCurve(&drawing, &next, second, false, 0.0, 8.0);
  QVERIFY(ids.has_value());
  QCOMPARE(drawing.points.size(), size_t{3});
  QCOMPARE(ids->front(), drawing.curves[0].points.back());
}

void CharacterEditTests::ExtendingAnEndGrowsItsCurve() {
  VectorDrawing drawing;
  int next = 1;
  const std::vector<QPointF> line = {{0, 0}, {10, 0}};
  const auto ids = AddCurve(&drawing, &next, line, false, 0.0, 0.0);
  const auto added = ExtendFrom(&drawing, &next, ids->back(), {20, 0}, 0.3);
  QVERIFY(added.has_value());
  QCOMPARE(drawing.curves.size(), size_t{1});
  QCOMPARE(drawing.curves[0].points.back(), *added);
  const auto branch = ExtendFrom(&drawing, &next, ids->back(), {10, 10}, 0.3);
  QVERIFY(branch.has_value());
  QCOMPARE(drawing.curves.size(), size_t{2});
}

void CharacterEditTests::DeletingPointsTidiesCurvesAndShapes() {
  VectorDrawing drawing;
  int next = 1;
  const std::vector<QPointF> square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  const auto ids = AddCurve(&drawing, &next, square, true, 0.0, 0.0);
  QVERIFY(MakeShape(&drawing, *ids, VectorShape{}).has_value());
  const std::vector<PointId> two = {(*ids)[0], (*ids)[1]};
  QVERIFY(DeletePoints(&drawing, two).has_value());
  QCOMPARE(drawing.curves.size(), size_t{1});
  QVERIFY(!drawing.curves[0].is_closed);
  QCOMPARE(drawing.shapes.size(), size_t{1});
  const std::vector<PointId> rest = {(*ids)[2]};
  QVERIFY(DeletePoints(&drawing, rest).has_value());
  QVERIFY(drawing.curves.empty());
  QVERIFY(drawing.shapes.empty());
}

void CharacterEditTests::WeldingTheEndsClosesALoop() {
  VectorDrawing drawing;
  int next = 1;
  const std::vector<QPointF> hook = {{0, 0}, {10, 0}, {10, 10}, {0, 1}};
  const auto ids = AddCurve(&drawing, &next, hook, false, 0.0, 0.0);
  QVERIFY(Weld(&drawing, ids->front(), ids->back()).has_value());
  QVERIFY(drawing.curves[0].is_closed);
  QCOMPARE(drawing.curves[0].points.size(), size_t{3});
}

void CharacterEditTests::ShapesComeFromWholeLines() {
  VectorDrawing drawing;
  int next = 1;
  const std::vector<QPointF> line = {{0, 0}, {10, 0}, {10, 10}};
  const auto ids = AddCurve(&drawing, &next, line, true, 0.0, 0.0);
  const std::vector<PointId> some = {(*ids)[0]};
  QVERIFY(!MakeShape(&drawing, some, VectorShape{}).has_value());
  QCOMPARE(*MakeShape(&drawing, *ids, VectorShape{}), 0);
  const std::vector<QPointF> places = {{0, 0}, {10, 0}, {10, 10}};
  QCOMPARE(ShapeAt(drawing, places, {7, 3}, 2.0), 0);
  QCOMPARE(ShapeAt(drawing, places, {50, 50}, 2.0), -1);
}

void CharacterEditTests::PartsMoveWithWhatTheyHold() {
  Character character = NewCharacter("Bo");
  const PartId group = *AddPart(&character, PartKind::kGroup, "Head", PartId());
  const PartId bones = character.parts[0].id;
  QVERIFY(MovePart(&character, bones, group, 0).has_value());
  QVERIFY(IsSound(character));
  QCOMPARE(FindPart(character, DrawingPart(character))->parent, bones);
  QCOMPARE(character.parts.front().id, group);
  QVERIFY(!MovePart(&character, group, DrawingPart(character), 0).has_value());
  QVERIFY(!MovePart(&character, group, bones, 0).has_value());
}

void CharacterEditTests::DuplicatesTakeNewIds() {
  Character character = NewCharacter("Bo");
  const PartId drawing = DrawingPart(character);
  const std::vector<QPointF> line = {{0, 0}, {10, 0}};
  QVERIFY(AddCurve(&FindPart(&character, drawing)->drawing,
                   &character.next_id, line, false, 0.0, 0.0)
              .has_value());
  const auto copy = DuplicatePart(&character, character.parts[0].id);
  QVERIFY(copy.has_value());
  QVERIFY(IsSound(character));
  QCOMPARE(character.parts.size(), size_t{4});
  QCOMPARE(FindPart(character, *copy)->name, QStringLiteral("Bones 2"));
}

void CharacterEditTests::RemovingABoneRehangsItsChildren() {
  Character character = NewCharacter("Bo");
  const PartId bones = character.parts[0].id;
  const BoneId a = *AddBone(&character, bones, {0, 0}, {10, 0}, BoneId());
  const BoneId b = *AddBone(&character, bones, {10, 0}, {20, 0}, a);
  const BoneId c = *AddBone(&character, bones, {20, 0}, {30, 0}, b);
  QVERIFY(SetSmart(&character, b, true).has_value());
  const std::vector<BoneId> gone = {b};
  QVERIFY(RemoveBones(&character, gone).has_value());
  QCOMPARE(FindBone(character, c)->parent, a);
  QVERIFY(character.actions.empty());
}

void CharacterEditTests::BoneParentsNeverLoop() {
  Character character = NewCharacter("Bo");
  const PartId bones = character.parts[0].id;
  const BoneId a = *AddBone(&character, bones, {0, 0}, {10, 0}, BoneId());
  const BoneId b = *AddBone(&character, bones, {10, 0}, {20, 0}, a);
  Bone looped = *FindBone(character, a);
  looped.parent = b;
  QVERIFY(!SetBone(&character, looped).has_value());
  QCOMPARE(FindBone(character, a)->parent, BoneId());
}

void CharacterEditTests::ActionStopsSortAndSkipRest() {
  Character character = NewCharacter("Bo");
  const BoneId smart =
      *AddBone(&character, character.parts[0].id, {0, 0}, {0, -50}, BoneId());
  QVERIFY(!AddActionStop(&character, smart, 30.0).has_value());
  QVERIFY(SetSmart(&character, smart, true).has_value());
  QVERIFY(!AddActionStop(&character, smart, 0.1).has_value());
  QCOMPARE(*AddActionStop(&character, smart, 60.0), 0);
  QCOMPARE(*AddActionStop(&character, smart, -60.0), 0);
  QCOMPARE(*AddActionStop(&character, smart, 60.2), 1);
  QCOMPARE(FindAction(character, smart)->stops.size(), size_t{2});
}

void CharacterEditTests::ManagerEditsAreNamedUndoSteps() {
  QTemporaryDir library;
  HistoryManager history(WithOneShot());
  CharacterManager characters(&history, library.path());
  const auto name = characters.Create("Bo");
  QVERIFY(name.has_value());
  QVERIFY(characters.Available().contains("Bo"));
  const Character* made = FindCharacter(history.current(), "Bo");
  const std::vector<QPointF> line = {{0, 0}, {50, 0}, {50, 50}};
  QVERIFY(characters.AddCurve("Bo", DrawingPart(*made), line, true, 0.0)
              .has_value());
  QCOMPARE(history.UndoLabel(), QStringLiteral("Draw line"));
  history.Undo();
  QVERIFY(FindCharacter(history.current(), "Bo")->parts[1].drawing.points.empty());
  QVERIFY(!characters.AddCurve("Nobody", PartId(2), line, false, 0.0)
               .has_value());
}

void CharacterEditTests::PosingKeysTheWholePose() {
  QTemporaryDir library;
  HistoryManager history(WithOneShot());
  CharacterManager characters(&history, library.path());
  StageManager stage(&history);
  QVERIFY(characters.Create("Bo").has_value());
  const auto layer = stage.AddCharacter(ShotId(1), "Bo");
  QVERIFY(layer.has_value());
  CharacterPoseManager poser(&history);
  const PartId drawing = DrawingPart(*poser.CharacterOf(ShotId(1), *layer));
  QVERIFY(poser
              .Pose(ShotId(1), *layer, Frame(10), "Move Drawing",
                    [&](const CharacterPose& pose) {
                      return WithPartShift(pose, drawing, {5, 0});
                    })
              .has_value());
  QCOMPARE(poser.HandPose(ShotId(1), *layer, Frame(20)).parts.at(drawing).offset,
           QPointF(5, 0));
  QCOMPARE(poser.HandPose(ShotId(1), *layer, Frame(0)).parts.at(drawing).offset,
           QPointF(5, 0));
  QVERIFY(poser.RemoveKey(ShotId(1), *layer, Frame(10)).has_value());
  QVERIFY(poser.HandPose(ShotId(1), *layer, Frame(10)).parts.empty());
}

void CharacterEditTests::TrimmedDropsRest() {
  CharacterPose pose;
  pose.parts[PartId(3)] = PartPose{};
  pose.parts[PartId(4)].points[PointId(9)] = QPointF();
  pose.bones[BoneId(5)].turn = 0.0;
  pose.bones[BoneId(6)].turn = 2.0;
  const CharacterPose trimmed = Trimmed(pose);
  QVERIFY(trimmed.parts.empty());
  QCOMPARE(trimmed.bones.size(), size_t{1});
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::CharacterEditTests)
#include "character_edit_tests.moc"
