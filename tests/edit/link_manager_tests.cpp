#include <QTest>

#include <cassert>
#include <memory>

#include "edit/history_manager.h"
#include "edit/link_manager.h"

namespace snapper {
namespace {

// A box, a picture, Bob (a character) and a cat picture on one shot.
Project Stage() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  const QStringList names = {"Box", "Picture", "Bob", "Cat"};
  for (int i = 0; i < 4; ++i) {
    Layer layer;
    layer.id = LayerId(i + 1);
    layer.name = names[i];
    layer.content = i == 2 ? LayerContent(CharacterLayer{"Bob", {}, false})
                           : LayerContent(ImageLayer{"/x.png"});
    shot.layers.push_back(layer);
  }
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots[0]->layers.size() == 4);
  return project;
}

const LinkEnd kBox{LayerId(1)};
const LinkEnd kPicture{LayerId(2)};
const LinkEnd kBob{LayerId(3)};
const LinkEnd kCat{LayerId(4)};

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
  QVERIFY(links.LinkTo(ShotId(1), {kPicture, kCat}, kBox, Frame(7))
              .has_value());
  QCOMPARE(OnlyShot(history).links.size(), size_t{2});
  const Link* cat = FindLink(OnlyShot(history), kCat);
  QVERIFY(cat != nullptr);
  QCOMPARE(cat->leader, kBox);
  QCOMPARE(cat->from, Frame(7));
  QCOMPARE(cat->strength, 1.0);
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
  QVERIFY(!links.WhyNoLink(ShotId(1), {kPicture}, {LayerId(9)}).isEmpty());
  // Following itself.
  QVERIFY(links.WhyNoLink(ShotId(1), {kBox}, kBox).contains("Box"));
  // A loop through another link; all or none.
  QVERIFY(links.LinkTo(ShotId(1), {kBox}, kPicture, Frame(0)).has_value());
  const QString loop = links.WhyNoLink(ShotId(1), {kCat, kPicture}, kBox);
  QVERIFY(loop.contains("loop"));
  QVERIFY(!links.LinkTo(ShotId(1), {kCat, kPicture}, kBox, Frame(0))
               .has_value());
  QCOMPARE(OnlyShot(history).links.size(), size_t{1});
}

void LinkManagerTests::StrengthShiftsAndStaysInRange() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(links.LinkTo(ShotId(1), {kPicture, kCat}, kBox, Frame(0))
              .has_value());
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture}, -0.75).has_value());
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture, kCat}, 0.5)
              .has_value());
  QCOMPARE(FindLink(OnlyShot(history), kPicture)->strength, 0.75);
  QCOMPARE(FindLink(OnlyShot(history), kCat)->strength, 1.5);
  QVERIFY(links.ShiftStrength(ShotId(1), {kPicture, kCat}, -5.0)
              .has_value());
  QCOMPARE(FindLink(OnlyShot(history), kCat)->strength, 0.0);
  QVERIFY(links.ShiftStrength(ShotId(1), {kCat}, 99.0).has_value());
  QCOMPARE(FindLink(OnlyShot(history), kCat)->strength, kMaxLinkStrength);
  QCOMPARE(history.UndoLabel(), QString("Link strength"));
  QVERIFY(!links.ShiftStrength(ShotId(1), {kBob}, 1.0).has_value());
}

void LinkManagerTests::UnlinkAndNames() {
  HistoryManager history(Stage());
  LinkManager links(&history);
  QVERIFY(!links.WhyNoUnlink(ShotId(1), {kCat}).isEmpty());
  QVERIFY(links.LinkTo(ShotId(1), {kCat}, kBox, Frame(0)).has_value());
  QVERIFY(links.WhyNoUnlink(ShotId(1), {kCat, kBob}).isEmpty());
  QVERIFY(links.Unlink(ShotId(1), {kCat, kBob}).has_value());
  QVERIFY(OnlyShot(history).links.empty());
  QCOMPARE(history.UndoLabel(), QString("Unlink movement"));
  QCOMPARE(links.NameOf(ShotId(1), kBox), QString("Box"));
  QCOMPARE(links.NameOf(ShotId(1), kCat), QString("Cat"));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::LinkManagerTests)
#include "link_manager_tests.moc"
