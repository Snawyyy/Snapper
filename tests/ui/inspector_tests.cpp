#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTest>

#include "anim/doll_pose.h"
#include "bench.h"
#include "inspector_stage.h"
#include "edit/link_manager.h"
#include "ui/drag_box.h"
#include "ui/fold_section.h"
#include "ui/kind_tile.h"
#include "ui/inspector.h"
#include "ui/shot_box.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/link_box.h"
#include "ui/motion_box.h"
#include "ui/point_motion_box.h"

namespace snapper {
namespace {

void Type(QDoubleSpinBox* box, double value) {
  box->setValue(value);
  emit box->editingFinished();
}

}  // namespace

class InspectorTests final : public QObject {
  Q_OBJECT

 private slots:
  void TypedPoseNumbersKeyAtThePlayhead();
  void CameraNumbersKeyTheCamera();
  void MotionLoopsGoOnThePick();
  void LayerBoxFollowsTheKind();
  void FieldsApplyAsYouTypeAsOneStep();
  void FieldsChangeEveryPickByTheSameAmount();
  void DragBoxTunesThePickedDot();
  void DragBoxSetsReachOfEveryPickedDot();
  void KindTilesShowTheRowsOfTheirKind();
  void PointMotionBoxAnimatesThePickedDot();
  void OnlyWhatIsPickedShows();
  void ShotBoxChangesThePickedShots();
  void LinkBoxShowsOnlyWhenLinked();
};

void InspectorTests::TypedPoseNumbersKeyAtThePlayhead() {
  Bench bench;
  Stage(&bench);
  bench.playback.Seek(Frame(5));
  PoseBox box(bench.All());
  QVERIFY(box.isEnabled());
  Type(box.findChild<QDoubleSpinBox*>("turn"), 45.0);
  QCOMPARE(Head(bench, 5).rotation, 45.0);
  QCOMPARE(Head(bench, 4).rotation, 45.0);
  bench.selection.Clear();
  QVERIFY(!box.isEnabled());
}

void InspectorTests::CameraNumbersKeyTheCamera() {
  Bench bench;
  Stage(&bench);
  CameraBox box(bench.All());
  Type(box.findChild<QDoubleSpinBox*>("zoom"), 2.5);
  QCOMPARE(bench.history.current().shots[0]->camera.keys[0].value.zoom, 2.5);
}

void InspectorTests::MotionLoopsGoOnThePick() {
  Bench bench;
  Stage(&bench);
  MotionBox box(bench.All());
  QPushButton* apply = box.findChild<QPushButton*>();
  QVERIFY(apply->isEnabled());
  apply->click();
  QCOMPARE(Head(bench, 2).offset, QPointF(0, 10));
  bench.selection.Clear();
  QVERIFY(!apply->isEnabled());
}

void InspectorTests::LayerBoxFollowsTheKind() {
  Bench bench;
  Stage(&bench);
  LayerBox box(bench.All());
  box.show();
  const auto words = box.findChild<QPlainTextEdit*>();
  QVERIFY(!words->isVisible());
  bench.selection.SelectLayer(LayerId(2));
  QVERIFY(words->isVisible());
  QCOMPARE(words->toPlainText(), QString("Hi"));
  const auto line = box.findChild<QLineEdit*>();
  line->setText("Lyric");
  emit line->editingFinished();
  QCOMPARE(bench.history.current().shots[0]->layers[1].name,
           QString("Lyric"));
}

void InspectorTests::FieldsApplyAsYouTypeAsOneStep() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  QDoubleSpinBox* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  QCOMPARE(Head(bench, 0).rotation, 10.0);
  turn->setValue(20.0);
  QCOMPARE(Head(bench, 0).rotation, 20.0);
  emit turn->editingFinished();
  QCOMPARE(bench.history.UndoLabel(), QString("Pose"));
  bench.history.Undo();
  QCOMPARE(Head(bench, 0).rotation, 0.0);
}

void InspectorTests::FieldsChangeEveryPickByTheSameAmount() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Bob");
  doll.rig.pieces.push_back({"arm", "", {}, 0, -1, {}});
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  const TrackRef arm{kShot, TrackKind::kPiece, LayerId(1), "arm"};
  QVERIFY(bench.pose.Move(arm, Frame(0), QPointF(10, 0)).has_value());
  bench.selection.SelectLayer(LayerId(1));
  bench.selection.PickThings({{LayerId(1), "head"}, {LayerId(1), "arm"}},
                             PickMode::kReplace, LayerId(1));
  PoseBox box(bench.All());
  auto* x = box.findChild<QDoubleSpinBox*>("x");
  QCOMPARE(x->value(), 10.0);
  x->setValue(15.0);
  emit x->editingFinished();
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(0));
  QCOMPARE(poses.at("arm").offset.x(), 15.0);
  QCOMPARE(poses.at("head").offset.x(), 5.0);
}

void InspectorTests::DragBoxTunesThePickedDot() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  DragBox box(bench.All());
  box.findChild<FoldSection*>("fine_tune")->SetOpen(true);
  auto* drag = box.findChild<KindTile*>("kind_drag");
  auto* lag = box.findChild<QDoubleSpinBox*>("lag");
  QVERIFY(!drag->isEnabled());
  bench.selection.PickDot(WarpDot{LayerId(1), "head", 4});
  QVERIFY(drag->isEnabled());
  QVERIFY(!lag->isEnabled());
  drag->click();
  QVERIFY(lag->isEnabled());
  Type(lag, 0.8);
  const auto& nodes =
      bench.history.current().dolls.at("Bob")->rig.pieces[0].drag_nodes;
  QCOMPARE(nodes.size(), size_t{1});
  QCOMPARE(nodes[0].lag, 0.8);
  QCOMPARE(drag->share(), Share::kAll);
}

void InspectorTests::DragBoxSetsReachOfEveryPickedDot() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  QVERIFY(bench.rig.SetWarpReach({{"Bob", "head", 0}}, 1.0).has_value());
  DragBox box(bench.All());
  auto* reach = box.findChild<QDoubleSpinBox*>("reach");
  QVERIFY(!reach->isEnabled());
  QVERIFY(!reach->toolTip().isEmpty());
  bench.selection.PickDots({{LayerId(1), "head", 0}, {LayerId(1), "head", 4}},
                           PickMode::kReplace);
  // Plain dots have a reach too; differing ones say Mixed.
  QVERIFY(reach->isEnabled());
  QCOMPARE(reach->findChild<QLineEdit*>()->text(), QString("Mixed"));
  Type(reach, 2.5);
  const auto& rig = bench.history.current().dolls.at("Bob")->rig.pieces[0];
  QCOMPARE(rig.warp_reach[0], 2.5);
  QCOMPARE(rig.warp_reach[4], 2.5);
  QCOMPARE(bench.history.UndoLabel(), QString("Set reach on 2 dots"));
}

void InspectorTests::KindTilesShowTheRowsOfTheirKind() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  DragBox box(bench.All());
  box.findChild<FoldSection*>("fine_tune")->SetOpen(true);
  auto* lag = box.findChild<QDoubleSpinBox*>("lag");
  auto* gravity = box.findChild<QDoubleSpinBox*>("gravity");
  QVERIFY(!gravity->isEnabled());
  QVERIFY(!gravity->toolTip().isEmpty());
  bench.selection.PickDot(WarpDot{LayerId(1), "head", 4});
  box.findChild<KindTile*>("kind_drag")->click();
  QVERIFY(lag->isVisibleTo(&box));
  QVERIFY(!gravity->isVisibleTo(&box));
  box.findChild<KindTile*>("kind_cloth")->click();
  QCOMPARE(bench.history.UndoLabel(), QString("Cloth 1 dot"));
  QVERIFY(!lag->isVisibleTo(&box));
  for (const char* name :
       {"stretch", "inertia", "air", "gravity", "spring"}) {
    QVERIFY(box.findChild<QDoubleSpinBox*>(name)->isVisibleTo(&box));
  }
  Type(gravity, 0.9);
  const auto& nodes =
      bench.history.current().dolls.at("Bob")->rig.pieces[0].drag_nodes;
  QCOMPARE(nodes[0].kind, NodeKind::kCloth);
  QCOMPARE(nodes[0].gravity, 0.9);
  box.findChild<KindTile*>("kind_drag")->click();
  QVERIFY(lag->isVisibleTo(&box));
  QCOMPARE(bench.history.current().dolls.at("Bob")->rig.pieces[0]
               .drag_nodes[0].kind, NodeKind::kDrag);
}

void InspectorTests::PointMotionBoxAnimatesThePickedDot() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  PointMotionBox box(bench.All());
  auto* kind = box.findChild<QComboBox*>("point_motion");
  auto* size = box.findChild<QDoubleSpinBox*>("motion_size");
  auto* angle = box.findChild<QDoubleSpinBox*>("motion_angle");
  QVERIFY(!kind->isEnabled());
  QVERIFY(!kind->toolTip().isEmpty());
  bench.selection.PickDot(WarpDot{LayerId(1), "head", 4});
  QVERIFY(kind->isEnabled());
  QVERIFY(!size->isEnabled());
  QVERIFY(!size->toolTip().isEmpty());
  kind->setCurrentIndex(static_cast<int>(WarpMotionKind::kWave));
  emit kind->activated(kind->currentIndex());
  QCOMPARE(bench.history.UndoLabel(), QString("Set how it moves on 1 dot"));
  QVERIFY(size->isEnabled());
  Type(size, 20.0);
  QCOMPARE(bench.history.UndoLabel(), QString("Set size on 1 dot"));
  const auto& motions =
      bench.history.current().dolls.at("Bob")->rig.pieces[0].point_motions;
  QCOMPARE(motions.size(), size_t{1});
  QCOMPARE(motions[0].point, 4);
  QCOMPARE(motions[0].size, 20.0);
  // A shiver shakes every way, so it has no direction.
  QVERIFY(angle->isEnabled());
  kind->setCurrentIndex(static_cast<int>(WarpMotionKind::kShiver));
  emit kind->activated(kind->currentIndex());
  QVERIFY(!angle->isEnabled());
  QCOMPARE(bench.history.current().dolls.at("Bob")->rig.pieces[0]
               .point_motions[0].size,
           20.0);
  kind->setCurrentIndex(0);
  emit kind->activated(0);
  QCOMPARE(bench.history.UndoLabel(),
           QString("Set how it moves on 1 dot"));
  QVERIFY(!size->isEnabled());
}

void InspectorTests::OnlyWhatIsPickedShows() {
  Bench bench;
  Stage(&bench);
  QVERIFY(bench.rig.SetWarpGrid("Bob", "head", {2, 2}).has_value());
  Inspector inspector(bench.All());
  const auto shown = [&inspector]<typename Box>() {
    return inspector.findChild<Box*>()->isVisibleTo(&inspector);
  };
  // A piece of a doll: its pose, layer, motion and saved poses.
  QVERIFY(shown.operator()<PoseBox>());
  QVERIFY(shown.operator()<PosesBox>());
  QVERIFY(!shown.operator()<ShotBox>());
  QVERIFY(!shown.operator()<CameraBox>());
  QVERIFY(!shown.operator()<DragBox>());
  bench.selection.PickDot(WarpDot{LayerId(1), "head", 0});
  QVERIFY(shown.operator()<DragBox>());
  QVERIFY(shown.operator()<PointMotionBox>());
  // Nothing picked: the shot and its camera.
  bench.selection.Clear();
  QVERIFY(shown.operator()<ShotBox>());
  QVERIFY(shown.operator()<CameraBox>());
  QVERIFY(!shown.operator()<PoseBox>());
  QVERIFY(!shown.operator()<DragBox>());
  QVERIFY(!shown.operator()<PointMotionBox>());
  // The text layer is not a doll: no saved poses.
  bench.selection.SelectLayer(LayerId(2));
  QVERIFY(shown.operator()<LayerBox>());
  QVERIFY(!shown.operator()<PosesBox>());
}

void InspectorTests::ShotBoxChangesThePickedShots() {
  Bench bench;
  Stage(&bench);
  // Stage made shot 1 by hand; new shots take ids after it.
  Project project = bench.history.current();
  project.next_shot_id = 2;
  bench.history.Reset(project);
  QVERIFY(bench.shots.Add(-1).has_value());
  bench.selection.SelectShot(kShot);
  ShotBox box(bench.All());
  auto* length = box.findChild<QSpinBox*>("shot_length");
  QCOMPARE(length->value(), 48);
  length->setValue(36);
  emit length->editingFinished();
  QCOMPARE(bench.history.current().shots[0]->length, Frame(36));
  auto* seconds = box.findChild<QDoubleSpinBox*>("shot_seconds");
  QCOMPARE(seconds->value(), 1.5);
  seconds->setValue(2.53);
  emit seconds->editingFinished();
  QCOMPARE(bench.history.current().shots[0]->length, Frame(61));
  QCOMPARE(length->value(), 61);
  // Transitions moved to the reel: the box has none.
  QVERIFY(box.findChild<QComboBox*>("transition") == nullptr);
  bench.selection.SelectShot(bench.history.current().shots[1]->id);
  auto* name = box.findChild<QLineEdit*>("shot_name");
  name->setText("Chorus");
  emit name->editingFinished();
  QCOMPARE(bench.history.current().shots[1]->name, QString("Chorus"));
}

void InspectorTests::LinkBoxShowsOnlyWhenLinked() {
  Bench bench;
  Stage(&bench);
  Inspector inspector(bench.All());
  const auto* link = inspector.findChild<LinkBox*>();
  QVERIFY(link != nullptr);
  // Bob's head is picked but follows nothing: no Link box.
  QVERIFY(!link->isVisibleTo(&inspector));
  const LinkEnd head{LayerId(1), "head"};
  QVERIFY(bench.links
              .LinkTo(kShot, {head}, LinkEnd{LayerId(2), {}}, Frame(0))
              .has_value());
  QVERIFY(link->isVisibleTo(&inspector));
  QVERIFY(bench.links.Unlink(kShot, {head}).has_value());
  QVERIFY(!link->isVisibleTo(&inspector));
}

}  // namespace snapper

QTEST_MAIN(snapper::InspectorTests)
#include "inspector_tests.moc"
