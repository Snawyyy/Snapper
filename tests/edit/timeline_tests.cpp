#include <QTest>

#include "edit/history_manager.h"
#include "edit/pose_manager.h"
#include "edit/timeline_rows.h"

namespace snapper {
namespace {

const ShotId kShot(1);

Project Stage() {
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}, {"arm", "", {}, 0, -1, {}}};
  Layer bob;
  bob.id = LayerId(1);
  bob.name = "Bob";
  bob.content = DollLayer{"Bob", {}, false};
  Layer flash;
  flash.id = LayerId(2);
  flash.name = "Flash";
  flash.content = EffectLayer{};
  Shot shot;
  shot.id = kShot;
  shot.layers = {bob, flash};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

}  // namespace

class TimelineTests final : public QObject {
  Q_OBJECT

 private slots:
  void OneRowPerLayerTopFirstThenCamera();
  void DollRowsShowWholePoses();
  void InPlaceKeysKeepTheLook();
};

void TimelineTests::OneRowPerLayerTopFirstThenCamera() {
  const auto rows = TimelineRows(Stage(), kShot);
  QCOMPARE(rows.size(), size_t{3});
  QCOMPARE(rows[0].name, QString("Flash"));
  QCOMPARE(rows[1].name, QString("Bob"));
  QCOMPARE(rows[2].name, QString("Camera"));
  QVERIFY(!rows[2].layer.IsValid());
  QVERIFY(TimelineRows(Stage(), ShotId(9)).empty());
}

void TimelineTests::DollRowsShowWholePoses() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  const TrackRef head{kShot, TrackKind::kPiece, LayerId(1), "head"};
  QVERIFY(pose.Rotate(head, Frame(4), 10.0).has_value());
  QVERIFY(pose.Rotate(head, Frame(8), 20.0).has_value());
  const auto rows = TimelineRows(history.current(), kShot);
  const TimelineRow& bob = rows[1];
  QCOMPARE(bob.keys, std::vector<Frame>({Frame(4), Frame(8)}));
  QCOMPARE(bob.is_eased, std::vector<bool>({false, false}));
  const auto keys = RowKeysAt(history.current(), bob, Frame(4));
  // Head, arm and the layer's own move.
  QCOMPARE(keys.size(), size_t{3});
  QVERIFY(RowKeysAt(history.current(), bob, Frame(5)).empty());
}

void TimelineTests::InPlaceKeysKeepTheLook() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.KeyInPlace(kShot, LayerId(1), Frame(6)).has_value());
  const auto rows = TimelineRows(history.current(), kShot);
  QCOMPARE(rows[1].keys, std::vector<Frame>({Frame(6)}));
  QCOMPARE(history.UndoLabel(), QString("Add key"));
  QVERIFY(pose.KeyInPlace(kShot, LayerId(2), Frame(1)).has_value());
  QCOMPARE(TimelineRows(history.current(), kShot)[0].keys.size(), size_t{1});
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::TimelineTests)
#include "timeline_tests.moc"
