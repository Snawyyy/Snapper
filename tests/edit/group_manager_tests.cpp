#include <QTest>

#include <cassert>
#include <memory>

#include "edit/group_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

const ShotId kOne(1);
const ShotId kTwo(2);
constexpr int kDots = 9;

// Bob on layer 1 of shot 1 and on layer 3 of shot 2; Cy on layer 2 of
// shot 1. Every piece has a 2 by 2 grid, so nine dots.
Project Stage() {
  Doll bob;
  bob.name = "Bob";
  bob.rig.pieces = {{"head", "", {}, 0, -1, {2, 2}},
                    {"arm", "", {}, 0, -1, {2, 2}}};
  Doll cy = bob;
  cy.name = "Cy";
  const auto layer = [](int id, const char* doll) {
    Layer made;
    made.id = LayerId(id);
    made.name = doll;
    made.content = DollLayer{doll, {}, false};
    return made;
  };
  Shot one;
  one.id = kOne;
  one.layers = {layer(1, "Bob"), layer(2, "Cy")};
  Shot two;
  two.id = kTwo;
  two.layers = {layer(3, "Bob")};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(bob);
  project.dolls["Cy"] = std::make_shared<const Doll>(cy);
  project.shots = {std::make_shared<const Shot>(one),
                   std::make_shared<const Shot>(two)};
  assert(project.shots.size() == 2);
  assert(kDots == 9);
  return project;
}

WarpDot Dot(int layer, const char* piece, int point) {
  assert(point >= 0 && point < kDots);
  assert(layer >= 1);
  return WarpDot{LayerId(layer), piece, point};
}

}  // namespace

class GroupManagerTests final : public QObject {
  Q_OBJECT

 private slots:
  void MakesANamedGroupInOneUndoStep();
  void RefusesAnEmptyPick();
  void RenamesAndRefusesAClash();
  void AddsAndRemovesDots();
  void DeletesAndUndoes();
  void ADefaultShowsInEveryShotOfTheDoll();
  void ADefaultCanGoBackToTheShot();
  void ADeadDotIsLeftOut();
  void PickingAGroupPicksItsPieces();
};

void GroupManagerTests::MakesANamedGroupInOneUndoStep() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  const Result<QString> name =
      groups.Create(kOne, {Dot(1, "head", 0), Dot(1, "head", 1)});
  QVERIFY(name.has_value());
  QCOMPARE(*name, QString("Group 1"));
  QCOMPARE(history.UndoLabel(), QString("Group 2 dots"));
  const auto shown = groups.GroupsOf(kOne);
  QCOMPARE(shown.size(), size_t(1));
  QCOMPARE(shown[0].dots.size(), size_t(2));
  QVERIFY(!shown[0].is_default);
  QVERIFY(groups.GroupsOf(kTwo).empty());
  QCOMPARE(*groups.Create(kOne, {Dot(1, "arm", 4)}), QString("Group 2"));
  history.Undo();
  history.Undo();
  QVERIFY(groups.GroupsOf(kOne).empty());
}

void GroupManagerTests::RefusesAnEmptyPick() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(!groups.WhyNoGroup(kOne, {}).isEmpty());
  QVERIFY(groups.WhyNoGroup(kOne, {Dot(1, "head", 0)}).isEmpty());
  QVERIFY(!groups.Create(kOne, {}).has_value());
  QVERIFY(!history.CanUndo());
}

void GroupManagerTests::RenamesAndRefusesAClash() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0)}).has_value());
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 1)}).has_value());
  QVERIFY(groups.Rename(kOne, "Group 1", "  Still   dots ").has_value());
  QCOMPARE(groups.GroupsOf(kOne)[0].name, QString("Still dots"));
  QVERIFY(!groups.Rename(kOne, "Group 2", "Still dots").has_value());
  QVERIFY(!groups.Rename(kOne, "Group 2", "   ").has_value());
  QVERIFY(!groups.Rename(kOne, "Nope", "X").has_value());
  QCOMPARE(groups.GroupsOf(kOne).size(), size_t(2));
}

void GroupManagerTests::AddsAndRemovesDots() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0)}).has_value());
  QVERIFY(groups.AddDots(kOne, "Group 1", {Dot(1, "head", 0), Dot(2, "arm", 3)})
              .has_value());
  QCOMPARE(groups.GroupsOf(kOne)[0].dots.size(), size_t(2));
  QVERIFY(!groups.AddDots(kOne, "Group 1", {}).has_value());
  QVERIFY(groups.RemoveDots(kOne, "Group 1", {Dot(1, "head", 0)}).has_value());
  QCOMPARE(groups.GroupsOf(kOne)[0].dots.size(), size_t(1));
  // A group with no dots left is gone.
  QVERIFY(groups.RemoveDots(kOne, "Group 1", {Dot(2, "arm", 3)}).has_value());
  QVERIFY(groups.GroupsOf(kOne).empty());
}

void GroupManagerTests::DeletesAndUndoes() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0)}).has_value());
  QVERIFY(groups.Delete(kOne, "Group 1").has_value());
  QCOMPARE(history.UndoLabel(), QString("Delete group Group 1"));
  QVERIFY(groups.GroupsOf(kOne).empty());
  history.Undo();
  QCOMPARE(groups.GroupsOf(kOne).size(), size_t(1));
  QVERIFY(!groups.Delete(kOne, "Nope").has_value());
}

void GroupManagerTests::ADefaultShowsInEveryShotOfTheDoll() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0), Dot(2, "head", 1)})
              .has_value());
  QVERIFY(groups.SetDefault(kOne, "Group 1", true).has_value());
  QVERIFY(!groups.SetDefault(kOne, "Group 1", true).has_value());
  // Kept on each doll, with no layer, and no longer in the shot.
  const Project& project = history.current();
  QCOMPARE(project.shots[0]->dot_groups.size(), size_t(0));
  const Rig& bob = project.dolls.at("Bob")->rig;
  QCOMPARE(bob.dot_groups.size(), size_t(1));
  QCOMPARE(bob.dot_groups[0].dots.size(), size_t(1));
  QVERIFY(!bob.dot_groups[0].dots[0].layer.IsValid());
  QCOMPARE(project.dolls.at("Cy")->rig.dot_groups.size(), size_t(1));
  // Shot 2 has Bob only: it shows Bob's dot, on its own layer.
  const auto in_two = groups.GroupsOf(kTwo);
  QCOMPARE(in_two.size(), size_t(1));
  QVERIFY(in_two[0].is_default);
  QCOMPARE(in_two[0].dots, (std::set<WarpDot>{Dot(3, "head", 0)}));
  // Shot 1 shows both dolls' dots.
  QCOMPARE(groups.GroupsOf(kOne)[0].dots,
           (std::set<WarpDot>{Dot(1, "head", 0), Dot(2, "head", 1)}));
  history.Undo();
  QVERIFY(groups.GroupsOf(kTwo).empty());
  QCOMPARE(groups.GroupsOf(kOne).size(), size_t(1));
}

void GroupManagerTests::ADefaultCanGoBackToTheShot() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0)}).has_value());
  QVERIFY(groups.SetDefault(kOne, "Group 1", true).has_value());
  QVERIFY(groups.AddDots(kOne, "Group 1", {Dot(1, "arm", 2)}).has_value());
  QCOMPARE(history.current().dolls.at("Bob")->rig.dot_groups[0].dots.size(),
           size_t(2));
  QVERIFY(groups.Rename(kOne, "Group 1", "Still").has_value());
  QCOMPARE(groups.GroupsOf(kTwo)[0].name, QString("Still"));
  QVERIFY(groups.SetDefault(kOne, "Still", false).has_value());
  QVERIFY(groups.GroupsOf(kTwo).empty());
  QVERIFY(history.current().dolls.at("Bob")->rig.dot_groups.empty());
  QVERIFY(!groups.GroupsOf(kOne)[0].is_default);
  QCOMPARE(groups.GroupsOf(kOne)[0].dots.size(), size_t(2));
}

void GroupManagerTests::ADeadDotIsLeftOut() {
  HistoryManager history(Stage());
  GroupManager groups(&history);
  QVERIFY(groups.Create(kOne, {Dot(1, "head", 0), Dot(2, "head", 1)})
              .has_value());
  Project next = history.current();
  Shot shot = *next.shots[0];
  shot.layers.pop_back();
  next.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Remove layer", next);
  QCOMPARE(groups.GroupsOf(kOne)[0].dots,
           (std::set<WarpDot>{Dot(1, "head", 0)}));
}

void GroupManagerTests::PickingAGroupPicksItsPieces() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  const std::set<WarpDot> group{Dot(1, "head", 0), Dot(1, "head", 1),
                                Dot(2, "arm", 3)};
  selection.PickGroup(group, PickMode::kReplace);
  QCOMPARE(selection.dots().size(), size_t(3));
  QCOMPARE(selection.PickedLayers().size(), size_t(2));
  // Ctrl on a group wholly picked takes its dots out, not its pieces.
  selection.PickGroup(group, PickMode::kToggle);
  QVERIFY(selection.dots().empty());
  QCOMPARE(selection.PickedLayers().size(), size_t(2));
  // Shift adds to what is picked.
  selection.PickGroup({Dot(1, "head", 5)}, PickMode::kReplace);
  selection.PickGroup({Dot(1, "head", 6)}, PickMode::kAdd);
  QCOMPARE(selection.dots().size(), size_t(2));
  selection.PickGroup({}, PickMode::kReplace);
  QCOMPARE(selection.dots().size(), size_t(2));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::GroupManagerTests)
#include "group_manager_tests.moc"
