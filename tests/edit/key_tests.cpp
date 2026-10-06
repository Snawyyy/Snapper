#include <QTest>

#include "edit/history_manager.h"
#include "edit/key_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const TrackRef kArm{kShot, TrackKind::kPiece, LayerId(1), "arm"};
const TrackRef kCamera{kShot, TrackKind::kCamera, {}, {}};

PiecePose Turned(double degrees) {
  PiecePose pose;
  pose.rotation = degrees;
  return pose;
}

// Arm keys at 0, 4 and 8 turned 0, 40 and 80 degrees; a camera key at 4.
Project Stage() {
  DollLayer posed{"Bob", {}, false};
  for (int i = 0; i < 3; ++i) {
    SetKey(&posed.pieces["arm"], {Frame(i * 4), Turned(i * 40.0),
                                  Ease::kStep});
  }
  Layer layer;
  layer.id = LayerId(1);
  layer.content = posed;
  Layer other = layer;
  other.id = LayerId(2);
  std::get<DollLayer>(other.content).pieces.clear();
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer, other};
  SetKey(&shot.camera, {Frame(4), CameraPose(), Ease::kStep});
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

std::vector<int> ArmFrames(const HistoryManager& history, int layer = 0) {
  std::vector<int> frames;
  const auto& pieces =
      std::get<DollLayer>(
          history.current().shots[0]->layers[static_cast<size_t>(layer)]
              .content)
          .pieces;
  const bool has_arm = pieces.contains("arm");
  if (has_arm) {
    for (const auto& key : pieces.at("arm").keys) {
      frames.push_back(key.frame.index());
    }
  }
  return frames;
}

}  // namespace

class KeyTests final : public QObject {
  Q_OBJECT

 private slots:
  void SpacingPutsKeysOnTwosAndThrees();
  void ShiftMovesAndReplaces();
  void RemoveAndEase();
  void RetimeMakesHoldsLongerAndShorter();
  void RepeatLoopsARange();
  void CopyPastesOntoAnotherLayer();
};

void KeyTests::ShiftMovesAndReplaces() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  const auto moved = keys.Shift({{kArm, Frame(0)}, {kCamera, Frame(4)}}, 4);
  QVERIFY(moved.has_value());
  QVERIFY(moved->contains({kArm, Frame(4)}));
  QCOMPARE(ArmFrames(history), std::vector<int>({4, 8}));
  QCOMPARE(std::get<DollLayer>(history.current().shots[0]->layers[0].content)
               .pieces.at("arm").keys[0].value.rotation,
           0.0);
  QCOMPARE(history.current().shots[0]->camera.keys[0].frame, Frame(8));
  QVERIFY(!keys.Shift({{kArm, Frame(4)}}, -5).has_value());
}

void KeyTests::RemoveAndEase() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  QVERIFY(keys.SetEase({{kArm, Frame(4)}}, Ease::kEaseOut).has_value());
  QCOMPARE(std::get<DollLayer>(history.current().shots[0]->layers[0].content)
               .pieces.at("arm").keys[1].ease,
           Ease::kEaseOut);
  QVERIFY(keys.Remove({{kArm, Frame(4)}, {kArm, Frame(8)}}).has_value());
  QCOMPARE(ArmFrames(history), std::vector<int>({0}));
  QVERIFY(!keys.Remove({}).has_value());
}

void KeyTests::RetimeMakesHoldsLongerAndShorter() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  QVERIFY(keys.Retime({kArm, kCamera}, Frame(2), 3).has_value());
  QCOMPARE(ArmFrames(history), std::vector<int>({0, 7, 11}));
  QCOMPARE(history.current().shots[0]->camera.keys[0].frame, Frame(7));
  QVERIFY(keys.Retime({kArm}, Frame(5), -3).has_value());
  QCOMPARE(ArmFrames(history), std::vector<int>({0, 8}));
  QVERIFY(!keys.Retime({kArm}, Frame(0), 0).has_value());
}

void KeyTests::RepeatLoopsARange() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  QVERIFY(keys.Repeat({kArm}, Frame(0), Frame(8), 2).has_value());
  QCOMPARE(ArmFrames(history),
           std::vector<int>({0, 4, 8, 12, 16, 20, 24}));
  QVERIFY(!keys.Repeat({kArm}, Frame(8), Frame(8), 2).has_value());
  QVERIFY(!keys.Repeat({kArm}, Frame(0), Frame(8), 0).has_value());
}

void KeyTests::CopyPastesOntoAnotherLayer() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  QCOMPARE(keys.WhyNoPaste(), QString("Copy some keys first."));
  QVERIFY(keys.Copy({{kArm, Frame(4)}, {kArm, Frame(8)}}).has_value());
  QVERIFY(keys.Paste(kShot, Frame(20), LayerId(2)).has_value());
  QCOMPARE(ArmFrames(history, 1), std::vector<int>({20, 24}));
  QVERIFY(keys.Paste(kShot, Frame(30), LayerId()).has_value());
  QCOMPARE(ArmFrames(history), std::vector<int>({0, 4, 8, 30, 34}));
  QVERIFY(keys.Copy({{kCamera, Frame(4)}}).has_value());
  QVERIFY(keys.Paste(kShot, Frame(0), LayerId(2)).has_value());
  QCOMPARE(history.current().shots[0]->camera.keys.size(), size_t{2});
}

void KeyTests::SpacingPutsKeysOnTwosAndThrees() {
  HistoryManager history(Stage());
  KeyManager keys(&history);
  QVERIFY(!keys.WhyNoSpace({{kArm, Frame(0)}}).isEmpty());
  QVERIFY(!keys.Space({{kArm, Frame(0)}}, 2).has_value());
  // The first two arm keys on 2s: the third, unpicked, slides back with
  // them so its hold stays four frames.
  const auto spaced = keys.Space({{kArm, Frame(0)}, {kArm, Frame(4)}}, 2);
  QVERIFY(spaced.has_value());
  QCOMPARE(ArmFrames(history), (std::vector<int>{0, 2, 6}));
  QCOMPARE(*spaced, (std::set<KeyRef>{{kArm, Frame(0)}, {kArm, Frame(2)}}));
  QCOMPARE(history.UndoLabel(), QString("Space keys on 2s"));
  // All three on 3s; the camera, not picked, stays put.
  QVERIFY(keys.Space({{kArm, Frame(0)}, {kArm, Frame(2)}, {kArm, Frame(6)}},
                     3)
              .has_value());
  QCOMPARE(ArmFrames(history), (std::vector<int>{0, 3, 6}));
  QCOMPARE(history.current().shots[0]->camera.keys[0].frame, Frame(4));
  QVERIFY(!keys.Space({{kArm, Frame(0)}, {kArm, Frame(3)}}, 0).has_value());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::KeyTests)
#include "key_tests.moc"
