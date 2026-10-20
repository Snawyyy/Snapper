#include <QTest>

#include <memory>

#include "anim/light_table.h"

namespace snapper {
namespace {

// Two shots, 10 frames then 6.
Project TwoShots() {
  Project project;
  for (const int length : {10, 6}) {
    auto shot = std::make_shared<Shot>();
    shot->length = Frame(length);
    project.shots.push_back(shot);
  }
  return project;
}

}  // namespace

class LightTableTests final : public QObject {
  Q_OBJECT

 private slots:
  void ShowsNeighboursNearestStrongest();
  void StopsAtTheShotEdge();
};

void LightTableTests::ShowsNeighboursNearestStrongest() {
  const auto skins = LightTableSkins(TwoShots(), Frame(5), 2);
  QCOMPARE(skins.size(), size_t{4});
  // Furthest first, so the nearest draw on top.
  QCOMPARE(skins[0], (OnionSkin{Frame(3), 0.3, true}));
  QCOMPARE(skins[1], (OnionSkin{Frame(7), 0.3, false}));
  QCOMPARE(skins[2], (OnionSkin{Frame(4), 0.6, true}));
  QCOMPARE(skins[3], (OnionSkin{Frame(6), 0.6, false}));
}

void LightTableTests::StopsAtTheShotEdge() {
  // Frame 10 opens the second shot: nothing of the first shows through.
  const auto first = LightTableSkins(TwoShots(), Frame(10), 2);
  QCOMPARE(first.size(), size_t{2});
  QVERIFY(!first[0].is_before && !first[1].is_before);
  const auto last = LightTableSkins(TwoShots(), Frame(15), 2);
  QCOMPARE(last.size(), size_t{2});
  QVERIFY(last[0].is_before && last[1].is_before);
  QVERIFY(LightTableSkins(TwoShots(), Frame(40), 2).empty());
}

}  // namespace snapper

QTEST_MAIN(snapper::LightTableTests)
#include "light_table_tests.moc"
