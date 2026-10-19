#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTest>

#include <cassert>
#include <cmath>

#include "bench.h"
#include "inspector_stage.h"
#include "edit/link_manager.h"
#include "ui/fold_section.h"
#include "ui/character_box.h"
#include "ui/inspector.h"
#include "ui/shot_box.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/link_box.h"

namespace snapper {
namespace {

void Type(QDoubleSpinBox* box, double value) {
  assert(box != nullptr);
  assert(std::isfinite(value));
  box->setValue(value);
  emit box->editingFinished();
}

}  // namespace

class InspectorTests final : public QObject {
  Q_OBJECT

 private slots:
  void TypedPoseNumbersKeyAtThePlayhead();
  void CameraNumbersKeyTheCamera();
  void LayerBoxFollowsTheKind();
  void FieldsApplyAsYouTypeAsOneStep();
  void FieldsChangeEveryPickByTheSameAmount();
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
  QCOMPARE(bench.history.current().shots[0]->camera.keys.size(), size_t{1});
  QCOMPARE(bench.history.current().shots[0]->camera.keys[0].value.zoom, 2.5);
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
  const TrackRef bob{kShot, TrackKind::kLayer, LayerId(1)};
  QVERIFY(bench.pose.Move(bob, Frame(0), QPointF(10, 0)).has_value());
  bench.selection.PickThings({Pick{LayerId(1)}, Pick{LayerId(2)}},
                             PickMode::kReplace, LayerId(1));
  PoseBox box(bench.All());
  auto* x = box.findChild<QDoubleSpinBox*>("x");
  QCOMPARE(x->value(), 10.0);
  x->setValue(15.0);
  emit x->editingFinished();
  const auto x_of = [&bench](size_t layer) {
    return Sample(bench.history.current().shots[0]->layers[layer].transform,
                  Frame(0), LayerPose())
        .offset.x();
  };
  QCOMPARE(x_of(0), 15.0);
  QCOMPARE(x_of(1), 5.0);
}

void InspectorTests::OnlyWhatIsPickedShows() {
  Bench bench;
  Stage(&bench);
  Inspector inspector(bench.All());
  const auto shown = [&inspector]<typename Box>() {
    return inspector.findChild<Box*>()->isVisibleTo(&inspector);
  };
  // A character: its pose, the character box and the layer's settings.
  QVERIFY(shown.operator()<PoseBox>());
  QVERIFY(shown.operator()<CharacterBox>());
  QVERIFY(shown.operator()<LayerBox>());
  QVERIFY(!shown.operator()<ShotBox>());
  QVERIFY(!shown.operator()<CameraBox>());
  // Nothing picked: the shot and its camera (folded closed at first).
  bench.selection.Clear();
  QVERIFY(shown.operator()<ShotBox>());
  QVERIFY(!inspector.findChild<CameraBox*>()->isHidden());
  QVERIFY(!shown.operator()<PoseBox>());
  // The text layer is not a character.
  bench.selection.SelectLayer(LayerId(2));
  QVERIFY(shown.operator()<LayerBox>());
  QVERIFY(!shown.operator()<CharacterBox>());
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
  // Bob is picked but follows nothing: no Link box.
  QVERIFY(!link->isVisibleTo(&inspector));
  const LinkEnd bob{LayerId(1)};
  QVERIFY(bench.links.LinkTo(kShot, {bob}, LinkEnd{LayerId(2)}, Frame(0))
              .has_value());
  QVERIFY(link->isVisibleTo(&inspector));
  QVERIFY(bench.links.Unlink(kShot, {bob}).has_value());
  QVERIFY(!link->isVisibleTo(&inspector));
}

}  // namespace snapper

QTEST_MAIN(snapper::InspectorTests)
#include "inspector_tests.moc"
