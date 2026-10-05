#include <QLineF>
#include <QTest>

#include "anim/doll_pose.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/pose_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const LayerId kDoll(1);
const LayerId kEffect(2);

// Two 10-pixel arm bones on each side, with an IK chain on the left.
Project Stage() {
  Doll doll;
  doll.art.pieces = {{"arm_l", {"a.png", "b.png"}, 0, {0, 0}, {10, 2}},
                     {"hand_l", {"h.png"}, 0, {10, 0}, {10, 2}},
                     {"arm_r", {"a.png"}, 0, {0, 0}, {10, 2}}};
  doll.rig.pieces = {{"arm_l", "", {0, 1}, 0, -1, {1, 1}},
                     {"hand_l", "arm_l", {0, 1}, 1, -1, {}},
                     {"arm_r", "", {0, 1}, 0, -1, {}}};
  doll.rig.chains = {{"reach", "arm_l", "hand_l", {10, 1}, true}};
  Layer layer;
  layer.id = kDoll;
  layer.content = DollLayer{"Bob", {}, false};
  Layer effect;
  effect.id = kEffect;
  effect.content = EffectLayer{};
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer, effect};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

TrackRef Piece(const QString& name) {
  return {kShot, TrackKind::kPiece, kDoll, name};
}

PoseMap PosesAt(const HistoryManager& history, Frame frame) {
  return SamplePoses(
      std::get<DollLayer>(history.current().shots[0]->layers[0].content),
      frame);
}

const Channel<PiecePose>& Keys(const HistoryManager& history,
                               const QString& piece) {
  return std::get<DollLayer>(
             history.current().shots[0]->layers[0].content)
      .pieces.at(piece);
}

}  // namespace

class PosingTests final : public QObject {
  Q_OBJECT

 private slots:
  void PosingKeysTheFrameAndHolds();
  void KeysKeepTheirEase();
  void DragsLandAsOneKey();
  void SwapsAndWarpsCheckTheRig();
  void CameraAndEffectsKeyToo();
  void IkBendsBothBones();
  void PasteMirroredSwapsSides();
};

void PosingTests::PosingKeysTheFrameAndHolds() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.Rotate(Piece("arm_l"), Frame(4), 30.0).has_value());
  QVERIFY(pose.Move(Piece("arm_l"), Frame(4), QPointF(2, 3)).has_value());
  QCOMPARE(Keys(history, "arm_l").keys.size(), size_t{1});
  QCOMPARE(PosesAt(history, Frame(10)).at("arm_l").rotation, 30.0);
  QCOMPARE(PosesAt(history, Frame(10)).at("arm_l").offset, QPointF(2, 3));
  QCOMPARE(Keys(history, "arm_l").keys[0].ease, Ease::kStep);
  QVERIFY(!pose.Rotate(Piece("arm_l"), Frame(4), qQNaN()).has_value());
  QVERIFY(!pose.Skew(Piece("arm_l"), Frame(4), 95.0).has_value());
  QVERIFY(!pose.SetOpacity(Piece("arm_l"), Frame(4), 2.0).has_value());
}

void PosingTests::KeysKeepTheirEase() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.Rotate(Piece("arm_l"), Frame(0), 10.0).has_value());
  Project eased = history.current();
  Shot shot = *eased.shots[0];
  std::get<DollLayer>(shot.layers[0].content).pieces["arm_l"].keys[0].ease =
      Ease::kLinear;
  eased.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Ease", eased);
  QVERIFY(pose.Rotate(Piece("arm_l"), Frame(0), 20.0).has_value());
  QCOMPARE(Keys(history, "arm_l").keys[0].ease, Ease::kLinear);
}

void PosingTests::DragsLandAsOneKey() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  {
    EditScope drag(&history, "Rotate arm_l");
    for (int degrees = 0; degrees <= 45; degrees += 5) {
      QVERIFY(pose.Rotate(Piece("arm_l"), Frame(2), degrees).has_value());
    }
  }
  QCOMPARE(history.UndoLabel(), QString("Rotate arm_l"));
  QCOMPARE(PosesAt(history, Frame(2)).at("arm_l").rotation, 45.0);
  history.Undo();
  QVERIFY(PosesAt(history, Frame(2)).empty());
}

void PosingTests::SwapsAndWarpsCheckTheRig() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.SwapDrawing(Piece("arm_l"), Frame(0), 1).has_value());
  QVERIFY(!pose.SwapDrawing(Piece("arm_l"), Frame(0), 2).has_value());
  QVERIFY(pose.Warp(Piece("arm_l"), Frame(0), 3, QPointF(1, 1)).has_value());
  QVERIFY(!pose.Warp(Piece("arm_l"), Frame(0), 4, QPointF()).has_value());
  QVERIFY(!pose.Warp(Piece("arm_r"), Frame(0), 0, QPointF()).has_value());
  const PiecePose arm = PosesAt(history, Frame(0)).at("arm_l");
  QCOMPARE(arm.drawing, 1);
  QCOMPARE(arm.warp.size(), size_t{4});
  QCOMPARE(arm.warp[3], QPointF(1, 1));
}

void PosingTests::CameraAndEffectsKeyToo() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  CameraPose camera;
  camera.zoom = 2.0;
  QVERIFY(pose.SetCamera(kShot, Frame(6), camera).has_value());
  camera.zoom = 0.0;
  QVERIFY(!pose.SetCamera(kShot, Frame(6), camera).has_value());
  const TrackRef amount{kShot, TrackKind::kEffectAmount, kEffect, {}};
  QVERIFY(pose.SetAmount(amount, Frame(1), 0.25).has_value());
  QVERIFY(!pose.SetAmount(amount, Frame(1), 1.5).has_value());
  QVERIFY(!pose.Rotate(amount, Frame(1), 5.0).has_value());
  const Shot& shot = *history.current().shots[0];
  QCOMPARE(shot.camera.keys[0].value.zoom, 2.0);
  QCOMPARE(std::get<EffectLayer>(shot.layers[1].content).amount.keys[0].value,
           0.25);
}

void PosingTests::IkBendsBothBones() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QVERIFY(pose.DragIk(kShot, kDoll, "reach", Frame(3), QPointF(10, 11))
              .has_value());
  const Project& project = history.current();
  const auto tip = PieceTransforms(*project.dolls.at("Bob"),
                                   PosesAt(history, Frame(3)))
                       .at("hand_l")
                       .map(QPointF(10, 1));
  QVERIFY(QLineF(tip, QPointF(10, 11)).length() < 1e-6);
  QCOMPARE(history.UndoLabel(), QString("Bend reach"));
  QVERIFY(!pose.DragIk(kShot, kDoll, "kick", Frame(3), QPointF()).has_value());
}

void PosingTests::PasteMirroredSwapsSides() {
  HistoryManager history(Stage());
  PoseManager pose(&history);
  QCOMPARE(pose.WhyNoPaste(), QString("Copy a pose first."));
  QVERIFY(pose.Rotate(Piece("arm_l"), Frame(0), 40.0).has_value());
  QVERIFY(pose.CopyPose(kShot, kDoll, Frame(0), {"arm_l"}).has_value());
  QVERIFY(pose.WhyNoPaste().isEmpty());
  QVERIFY(pose.PastePose(kShot, kDoll, Frame(8), true).has_value());
  const PoseMap at8 = PosesAt(history, Frame(8));
  QCOMPARE(at8.at("arm_r").rotation, -40.0);
  QCOMPARE(at8.at("arm_l").rotation, 40.0);
  QVERIFY(pose.PastePose(kShot, kDoll, Frame(12), false).has_value());
  QCOMPARE(Keys(history, "arm_l").keys.back().frame, Frame(12));
  QVERIFY(!pose.CopyPose(kShot, kEffect, Frame(0), {}).has_value());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::PosingTests)
#include "posing_tests.moc"
