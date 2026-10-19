#include <QTest>

#include <cassert>

#include "anim/sampler.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/pose_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const LayerId kBob(1);
const LayerId kEffect(2);
const LayerId kAmy(3);

// Bob and Amy, two characters, with an effect between them.
Project Stage() {
  Layer bob;
  bob.id = kBob;
  bob.content = CharacterLayer{"Bob", {}, false};
  Layer effect;
  effect.id = kEffect;
  effect.content = EffectLayer{};
  Layer amy;
  amy.id = kAmy;
  amy.content = CharacterLayer{"Amy", {}, false};
  Shot shot;
  shot.id = kShot;
  shot.layers = {bob, effect, amy};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots.size() == 1);
  return project;
}

TrackRef Track(LayerId layer) {
  assert(layer.IsValid());
  return TrackRef{kShot, TrackKind::kLayer, layer};
}

const Layer& LayerOf(const HistoryManager& history, LayerId id) {
  assert(!history.current().shots.empty());
  const Layer* layer = FindLayer(*history.current().shots[0], id);
  assert(layer != nullptr);
  return *layer;
}

LayerPose PoseAt(const HistoryManager& history, LayerId id, Frame frame) {
  assert(frame.index() >= 0);
  return Sample(LayerOf(history, id).transform, frame, LayerPose());
}

}  // namespace

class PosingTests final : public QObject {
  Q_OBJECT

 private slots:
  void PosingKeysTheFrameAndHolds();
  void KeysKeepTheirEase();
  void DragsLandAsOneKey();
  void CameraAndEffectsKeyToo();
  void ShiftMovesEachByTheSameAmount();
};

void PosingTests::PosingKeysTheFrameAndHolds() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.Rotate(Track(kBob), Frame(4), 30.0).has_value());
  QVERIFY(pose.Move(Track(kBob), Frame(4), QPointF(2, 3)).has_value());
  QCOMPARE(LayerOf(history, kBob).transform.keys.size(), size_t{1});
  QCOMPARE(PoseAt(history, kBob, Frame(10)).rotation, 30.0);
  QCOMPARE(PoseAt(history, kBob, Frame(10)).offset, QPointF(2, 3));
  QCOMPARE(LayerOf(history, kBob).transform.keys[0].ease, Ease::kStep);
  QVERIFY(!pose.Rotate(Track(kBob), Frame(4), qQNaN()).has_value());
  QVERIFY(!pose.Skew(Track(kBob), Frame(4), 95.0).has_value());
  QVERIFY(!pose.SetOpacity(Track(kBob), Frame(4), 2.0).has_value());
}

void PosingTests::KeysKeepTheirEase() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.Rotate(Track(kBob), Frame(0), 10.0).has_value());
  Project eased = history.current();
  Shot shot = *eased.shots[0];
  shot.layers[0].transform.keys[0].ease = Ease::kLinear;
  eased.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Ease", eased);
  QVERIFY(pose.Rotate(Track(kBob), Frame(0), 20.0).has_value());
  QCOMPARE(LayerOf(history, kBob).transform.keys[0].ease, Ease::kLinear);
}

void PosingTests::DragsLandAsOneKey() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  {
    EditScope drag(&history, "Rotate Bob");
    for (int degrees = 0; degrees <= 45; degrees += 5) {
      QVERIFY(pose.Rotate(Track(kBob), Frame(2), degrees).has_value());
    }
  }
  QCOMPARE(history.UndoLabel(), QString("Rotate Bob"));
  QCOMPARE(PoseAt(history, kBob, Frame(2)).rotation, 45.0);
  history.Undo();
  QVERIFY(LayerOf(history, kBob).transform.keys.empty());
}

void PosingTests::CameraAndEffectsKeyToo() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  CameraPose camera;
  camera.zoom = 2.0;
  QVERIFY(pose.SetCamera(kShot, Frame(6), camera).has_value());
  camera.zoom = 0.0;
  QVERIFY(!pose.SetCamera(kShot, Frame(6), camera).has_value());
  const TrackRef amount{kShot, TrackKind::kEffectAmount, kEffect};
  QVERIFY(pose.SetAmount(amount, Frame(1), 0.25).has_value());
  QVERIFY(!pose.SetAmount(amount, Frame(1), 1.5).has_value());
  QVERIFY(!pose.Rotate(amount, Frame(1), 5.0).has_value());
  const Shot& shot = *history.current().shots[0];
  QCOMPARE(shot.camera.keys[0].value.zoom, 2.0);
  QCOMPARE(std::get<EffectLayer>(shot.layers[1].content).amount.keys[0].value,
           0.25);
}

void PosingTests::ShiftMovesEachByTheSameAmount() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.Move(Track(kBob), Frame(0), QPointF(10, 0)).has_value());
  PoseDelta delta;
  delta.offset = QPointF(5, 1);
  delta.scale_x = -2.0;
  delta.opacity = 0.5;
  QVERIFY(pose.Shift({Track(kBob), Track(kAmy)}, Frame(0), delta)
              .has_value());
  QCOMPARE(PoseAt(history, kBob, Frame(0)).offset, QPointF(15, 1));
  QCOMPARE(PoseAt(history, kAmy, Frame(0)).offset, QPointF(5, 1));
  QCOMPARE(PoseAt(history, kAmy, Frame(0)).scale_x, 0.0);
  QCOMPARE(PoseAt(history, kAmy, Frame(0)).opacity, 1.0);
  QVERIFY(!pose.Shift({}, Frame(0), delta).has_value());
  history.Undo();
  QCOMPARE(PoseAt(history, kBob, Frame(0)).offset, QPointF(10, 0));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::PosingTests)
#include "posing_tests.moc"
