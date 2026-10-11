#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <cassert>

#include "io/doll_json.h"
#include "io/dot_group_json.h"
#include "io/layer_json.h"

namespace snapper {
namespace {

DotGroup Group(const QString& name, int layer, const QString& piece,
               std::vector<int> points) {
  assert(!name.isEmpty());
  assert(!points.empty());
  DotGroup group;
  group.name = name;
  for (const int point : points) {
    group.dots.push_back({LayerId(layer), piece, point});
  }
  return group;
}

}  // namespace

class DotGroupJsonTests : public QObject {
  Q_OBJECT

 private slots:
  void ShotGroupsRoundTrip();
  void DollDefaultsRoundTrip();
  void DamagedGroupsAreDropped();
  void FreshNameSkipsTheOnesInUse();
};

void DotGroupJsonTests::ShotGroupsRoundTrip() {
  Shot shot;
  shot.id = ShotId(1);
  shot.dot_groups = {Group("Still", 2, "arm", {0, 1, 5}),
                     Group("Jelly", 3, "hair", {4})};
  const QByteArray text = QJsonDocument(ShotToJson(shot)).toJson();
  JsonIssues issues;
  const Shot back =
      ShotFromJson(QJsonDocument::fromJson(text).object(), &issues);
  QVERIFY(!issues.HasIssue());
  QCOMPARE(back.dot_groups.size(), size_t(2));
  QVERIFY(back.dot_groups == shot.dot_groups);
}

void DotGroupJsonTests::DollDefaultsRoundTrip() {
  Rig rig;
  rig.dot_groups = {Group("Still", 0, "arm", {0, 1})};
  const QByteArray text = QJsonDocument(RigToJson(rig)).toJson();
  JsonIssues issues;
  const Rig back =
      RigFromJson(QJsonDocument::fromJson(text).object(), &issues);
  QVERIFY(!issues.HasIssue());
  QVERIFY(back.dot_groups == rig.dot_groups);
  QVERIFY(!back.dot_groups[0].dots[0].layer.IsValid());
}

void DotGroupJsonTests::DamagedGroupsAreDropped() {
  const QJsonObject good{{"name", "A"},
                         {"dots", QJsonArray{QJsonObject{{"layer", 1},
                                                         {"piece", "x"},
                                                         {"point", 2}},
                                             // A repeat, no point, no piece.
                                             QJsonObject{{"layer", 1},
                                                         {"piece", "x"},
                                                         {"point", 2}},
                                             QJsonObject{{"layer", 1},
                                                         {"piece", "x"}},
                                             QJsonObject{{"layer", 1},
                                                         {"point", 3}}}}};
  const QJsonArray array{good, good, QJsonObject{{"name", ""}}};
  JsonIssues issues;
  const std::vector<DotGroup> groups = DotGroupsFromJson(array, &issues);
  QCOMPARE(groups.size(), size_t(1));
  QCOMPARE(groups[0].dots.size(), size_t(1));
  QVERIFY(DotGroupsFromJson(QJsonValue(), &issues).empty());
}

void DotGroupJsonTests::FreshNameSkipsTheOnesInUse() {
  std::vector<DotGroup> groups;
  QCOMPARE(FreshGroupName(groups), QString("Group 1"));
  groups.push_back(Group("Group 1", 1, "a", {0}));
  groups.push_back(Group("Group 3", 1, "a", {0}));
  QCOMPARE(FreshGroupName(groups), QString("Group 2"));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::DotGroupJsonTests)
#include "dot_group_json_tests.moc"
