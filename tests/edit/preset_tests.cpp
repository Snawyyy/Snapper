#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "edit/preset_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const LayerId kLayer(1);
const TrackRef kHead{kShot, TrackKind::kPiece, kLayer, "head"};

Project Stage() {
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}, {"arm", "", {}, 0, -1, {}}};
  DollLayer posed{"Bob", {}, false};
  PiecePose tilted;
  tilted.rotation = 10.0;
  SetKey(&posed.pieces["head"], {Frame(0), tilted, Ease::kStep});
  Layer layer;
  layer.id = kLayer;
  layer.content = posed;
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots.size() == 1);
  assert(project.dolls.size() == 1);
  return project;
}

PoseMap At(const HistoryManager& history, int frame) {
  assert(frame >= 0);
  assert(!history.current().shots.empty());
  return SamplePoses(
      std::get<DollLayer>(history.current().shots[0]->layers[0].content),
      Frame(frame));
}

}  // namespace

class PresetTests final : public QObject {
  Q_OBJECT

 private slots:
  void SavedPosesComeBack();
  void DamagedPoseFilesAreNeverOverwritten();
};

void PresetTests::SavedPosesComeBack() {
  QTemporaryDir dir;
  const QString file = QDir(dir.path()).filePath("poses.json");
  HistoryManager history(Stage());
  {
    PresetManager presets(&history, file);
    QVERIFY(presets.SavePose("Tilt", kShot, kLayer, Frame(0)).has_value());
    QVERIFY(!presets.SavePose(" ", kShot, kLayer, Frame(0)).has_value());
  }
  PresetManager presets(&history, file);
  QCOMPARE(presets.SavedPoses(), QStringList({"Tilt"}));
  QVERIFY(presets.load_error().isEmpty());
  Project cleared = history.current();
  Shot shot = *cleared.shots[0];
  std::get<DollLayer>(shot.layers[0].content).pieces.clear();
  cleared.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Clear", cleared);
  QVERIFY(presets.ApplyPose("Tilt", kShot, kLayer, Frame(9)).has_value());
  QCOMPARE(At(history, 9).at("head").rotation, 10.0);
  QVERIFY(!presets.ApplyPose("Wave", kShot, kLayer, Frame(9)).has_value());
  QVERIFY(presets.DeletePose("Tilt").has_value());
  QVERIFY(presets.SavedPoses().isEmpty());
}

void PresetTests::DamagedPoseFilesAreNeverOverwritten() {
  QTemporaryDir dir;
  const QString file = QDir(dir.path()).filePath("poses.json");
  QFile broken(file);
  QVERIFY(broken.open(QIODevice::WriteOnly));
  broken.write("{ broken");
  broken.close();
  HistoryManager history(Stage());
  PresetManager presets(&history, file);
  QVERIFY(!presets.load_error().isEmpty());
  QVERIFY(!presets.SavePose("Tilt", kShot, kLayer, Frame(0)).has_value());
  QVERIFY(broken.open(QIODevice::ReadOnly));
  QCOMPARE(broken.readAll(), QByteArray("{ broken"));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::PresetTests)
#include "preset_tests.moc"
