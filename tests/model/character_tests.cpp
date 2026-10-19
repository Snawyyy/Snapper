#include <QTest>

#include "model/character.h"

namespace snapper {
namespace {

CharacterPart Part(int id, PartKind kind, int parent) {
  CharacterPart part;
  part.id = PartId(id);
  part.name = QStringLiteral("part %1").arg(id);
  part.kind = kind;
  part.parent = PartId(parent);
  return part;
}

// Bones (1) > Body group (2) > Arm vector (3); Mouth switch (4) on top.
Character Tree() {
  Character character;
  character.parts = {Part(1, PartKind::kBone, 0), Part(2, PartKind::kGroup, 1),
                     Part(3, PartKind::kVector, 2),
                     Part(4, PartKind::kSwitch, 0)};
  character.parts[0].bones = {Bone{.id = BoneId(5), .name = "spine"}};
  character.next_id = 6;
  return character;
}

}  // namespace

class CharacterTests final : public QObject {
  Q_OBJECT

 private slots:
  void TreeIsSound();
  void RepeatedIdIsNotSound();
  void ChildBeforeParentIsNotSound();
  void OnlyFoldersHoldAndNoLoops();
  void BoneParentIsTheNearestBonePart();
  void BonesAreFoundWithTheirPart();
};

void CharacterTests::TreeIsSound() {
  QVERIFY(IsSound(Tree()));
  QCOMPARE(ChildrenOf(Tree(), PartId()).size(), size_t{2});
}

void CharacterTests::RepeatedIdIsNotSound() {
  Character character = Tree();
  character.parts[0].bones[0].id = BoneId(3);
  QVERIFY(!IsSound(character));
}

void CharacterTests::ChildBeforeParentIsNotSound() {
  Character character = Tree();
  std::swap(character.parts[1], character.parts[2]);
  QVERIFY(!IsSound(character));
}

void CharacterTests::OnlyFoldersHoldAndNoLoops() {
  const Character character = Tree();
  QVERIFY(CanHold(character, PartId(3), PartId(4)));
  QVERIFY(!CanHold(character, PartId(4), PartId(3)));
  QVERIFY(!CanHold(character, PartId(1), PartId(2)));
  QVERIFY(CanHold(character, PartId(2), PartId()));
}

void CharacterTests::BoneParentIsTheNearestBonePart() {
  const Character character = Tree();
  QCOMPARE(BoneParentOf(character, PartId(3))->id, PartId(1));
  QVERIFY(BoneParentOf(character, PartId(4)) == nullptr);
  QVERIFY(BoneParentOf(character, PartId(1)) == nullptr);
}

void CharacterTests::BonesAreFoundWithTheirPart() {
  const Character character = Tree();
  QCOMPARE(FindBone(character, BoneId(5))->name, QStringLiteral("spine"));
  QCOMPARE(BoneOwner(character, BoneId(5))->id, PartId(1));
  QVERIFY(FindBone(character, BoneId(9)) == nullptr);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::CharacterTests)
#include "character_tests.moc"
