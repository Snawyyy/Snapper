#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTest>

#include "anim/doll_pose.h"
#include "bench.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/motion_box.h"

namespace snapper {
namespace {

const ShotId kShot(1);

void Stage(Bench* bench) {
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}};
  Layer bob;
  bob.id = LayerId(1);
  bob.name = "Bob";
  bob.content = DollLayer{"Bob", {}, false};
  Layer words;
  words.id = LayerId(2);
  words.name = "Words";
  words.content = TextLayer{};
  std::get<TextLayer>(words.content).text = "Hi";
  Shot shot;
  shot.id = kShot;
  shot.layers = {bob, words};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  bench->selection.SelectLayer(LayerId(1));
  bench->selection.SelectPieces({"head"}, false);
}

void Type(QDoubleSpinBox* box, double value) {
  box->setValue(value);
  emit box->editingFinished();
}

PiecePose Head(const Bench& bench, int frame) {
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(frame));
  const auto found = poses.find("head");
  return found == poses.end() ? PiecePose() : found->second;
}

}  // namespace

class InspectorTests final : public QObject {
  Q_OBJECT

 private slots:
  void TypedPoseNumbersKeyAtThePlayhead();
  void CameraNumbersKeyTheCamera();
  void MotionLoopsGoOnThePick();
  void LayerBoxFollowsTheKind();
};

void InspectorTests::TypedPoseNumbersKeyAtThePlayhead() {
  Bench bench;
  Stage(&bench);
  bench.playback.Seek(Frame(5));
  PoseBox box(bench.All());
  QVERIFY(box.isEnabled());
  Type(box.findChildren<QDoubleSpinBox*>()[0], 45.0);
  QCOMPARE(Head(bench, 5).rotation, 45.0);
  QCOMPARE(Head(bench, 4).rotation, 45.0);
  bench.selection.Clear();
  QVERIFY(!box.isEnabled());
}

void InspectorTests::CameraNumbersKeyTheCamera() {
  Bench bench;
  Stage(&bench);
  CameraBox box(bench.All());
  Type(box.findChildren<QDoubleSpinBox*>()[2], 2.5);
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

}  // namespace snapper

QTEST_MAIN(snapper::InspectorTests)
#include "inspector_tests.moc"
