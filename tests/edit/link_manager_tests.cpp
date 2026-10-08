#include <QTest>

#include <cassert>
#include <memory>

#include "edit/history_manager.h"
#include "edit/link_manager.h"

namespace snapper {
namespace {

// A box, a picture and Bob (a body with an arm) on one shot.
Project Stage() {
  Doll doll;
  doll.name = "Bob";
  doll.art.pieces = {{"body", {"body.png"}, 0, {0, 0}, {20, 40}},
                     {"arm", {"arm.png"}, 0, {20, 0}, {30, 8}}};
  doll.rig.pieces = {{"body", "", {10, 20}, 0, -1, {}},
                     {"arm", "body", {0, 4}, 1, -1, {}}};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  Shot shot;
  shot.id = ShotId(1);
  const QStringList names = {"Box", "Picture", "Bob"};
  for (int i = 0; i < 3; ++i) {
    Layer layer;
    layer.id = LayerId(i + 1);
    layer.name = names[i];
    layer.content = i == 2 ? LayerContent(DollLayer{"Bob", {}, false})
                           : LayerContent(ImageLayer{"/x.png"});
    shot.layers.push_back(layer);
  }
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots[0]->layers.size() == 3);
  assert(FindDoll(project, "Bob") != nullptr);
  return project;
}

const LinkEnd kBox{LayerId(1), {}};
const LinkEnd kPicture{LayerId(2), {}};
const LinkEnd kBob{LayerId(3), {}};
const LinkEnd kArm{LayerId(3), "arm"};

const Shot& OnlyShot(const HistoryManager& history) {
  assert(history.current().shots.size() == 1);
  assert(history.current().shots[0] != nullptr);
  return *history.current().shots[0];
}

}  // namespace

class LinkManagerTests final : public QObject {
  Q_OBJECT

 private slots:
  void LinksManyAtOnce();
  void RelinkingReplaces();
  void RefusesWhatCantFollow();
  void StrengthShiftsAndStaysInRange();
  void UnlinkAndNames();
};

void LinkManagerTests::LinksManyAtOnce() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(links.LinkTo(ShotId(1), {kPicture, kArm}, kBox, Frame(7))
              .has_value());
  QCOMPARE(OnlyShot(history).links.size(), size_t{2});
  const Link* arm = FindLink(OnlyShot(history), kArm);
  QVERIFY(arm != nullptr);
  QCOMPARE(arm->leader, kBox);
  QCOMPARE(arm->from, Frame(7));
  QCOMPARE(arm->strength, 1.0);
  QCOMPARE(history.UndoLabel(), QString("Link movement"));
  history.Undo();
  QVERIFY(OnlyShot(history).links.empty());
}

void LinkManagerTests::RelinkingReplaces() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(links.LinkTo(ShotId(1), {kPicture}, kBox, Frame(0)).has_value());
  QVERIFY(links.LinkTo(ShotId(1), {kPicture}, kBob, Frame(3)).has_value());
  QCOMPARE(OnlyShot(history).links.size(), size_t{1});
  QCOMPARE(FindLink(OnlyShot(history), kPicture)->leader, kBob);
}

void LinkManagerTests::RefusesWhatCantFollow() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(!links.WhyNoLink(ShotId(1), {}, kBox).isEmpty());
  QVERIFY(!links.WhyNoLink(ShotId(9), {kPicture}, kBox).isEmpty());
  QVERIFY(!links.WhyNoLink(ShotId(1), {kPicture}, {LayerId(9), {}})
               .isEmpty());
  QVERIFY(!links.WhyNoLink(ShotId(1), {kPicture}, {LayerId(3), "tail"})
               .isEmpty());
  // Following itself or its own doll.
  QVERIFY(links.WhyNoLink(ShotId(1), {kBox}, kBox).contains("Box"));
  QVERIFY(!links.WhyNoLink(ShotId(1), {kBob}, kArm).isEmpty());
  // A loop through another link; all or none.
  QVERIFY(links.LinkTo(ShotId(1), {kBox}, kPicture, Frame(0)).has_value());
  const QString loop = links.WhyNoLink(ShotId(1), {kArm, kPicture}, kBox);
  QVERIFY(loop.contains("loop"));
  QVERIFY(!links.LinkTo(ShotId(1), {kArm, kPicture}, kBox, Frame(0))
               .has_value());
  QCOMPARE(OnlyShot(history).links.size(), size_t{1});
}

void LinkManagerTests::StrengthShiftsAndStaysInRange() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(links.LinkTo(ShotId(1), {kPicture, kArm}, kBox, Frame(0))
              .has_value());
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture}, -0.75).has_value());
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture, kArm}, 0.5)
              .has_value());
  QCOMPARE(FindLink(OnlyShot(history), kPicture)->strength, 0.75);
  QCOMPARE(FindLink(OnlyShot(history), kArm)->strength, 1.5);
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture, kArm}, -5.0)
              .has_value());
  QCOMPARE(FindLink(OnlyShot(history), kArm)->strength, 0.0);
  QVERIFY(links.ShiftStrength(ShotId(1), {kArm}, 99.0).has_value());
  QCOMPARE(FindLink(OnlyShot(history), kArm)->strength, kMaxLinkStrength);
  QCOMPARE(history.UndoLabel(), QString("Link strength"));
  QVERIFY(!links.ShiftStrength(ShotId(1), {kBob}, 1.0).has_value());
}

void LinkManagerTests::UnlinkAndNames() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(!links.WhyNoUnlink(ShotId(1), {kArm}).isEmpty());
  QVERIFY(links.LinkTo(ShotId(1), {kArm}, kBox, Frame(0)).has_value());
  QVERIFY(links.WhyNoUnlink(ShotId(1), {kArm, kBob}).isEmpty());
  QVERIFY(links.Unlink(ShotId(1), {kArm, kBob}).has_value());
  QVERIFY(OnlyShot(history).links.empty());
  QCOMPARE(history.UndoLabel(), QString("Unlink movement"));
  QCOMPARE(links.NameOf(ShotId(1), kBox), QString("Box"));
  QCOMPARE(links.NameOf(ShotId(1), kArm), QString("Bob's arm"));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::LinkManagerTests)
#include "link_manager_tests.moc"
