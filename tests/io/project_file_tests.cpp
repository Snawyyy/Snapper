#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "io/pose_file.h"
#include "io/project_file.h"

namespace snapper {
namespace {

Layer MakeLayer(int id, LayerContent content) {
  Layer layer;
  layer.id = LayerId(id);
  layer.name = QString("layer %1").arg(id);
  layer.start = Frame(2);
  layer.length = Frame(10);
  layer.is_visible = id % 2 == 0;
  PiecePose moved;
  moved.offset = QPointF(3, 4);
  moved.warp = {QPointF(1, 2)};
  SetKey(&layer.transform, {Frame(5), moved, Ease::kEaseOut});
  layer.content = std::move(content);
  return layer;
}

// One of everything, so a field the file forgets fails the test.
Project FullProject() {
  Project project;
  project.name = "Teto";
  project.canvas = {1080, 1920};
  project.song = "/music/song.flac";
  project.next_shot_id = 3;
  project.next_layer_id = 9;
  Doll doll;
  doll.name = "Bob";
  doll.folder = "/dolls/Bob.doll";
  doll.art.canvas = QSize(100, 80);
  doll.art.pieces = {{"head", {"a.png", "b.png"}, 1, {-5, -5}, {10, 10}}};
  doll.rig.pieces = {{"head", "", {5, 5}, 2, 1, {2, 2}}};
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);

  DollLayer posed{"Bob", {}, true};
  PiecePose tilt;
  tilt.rotation = 15.0;
  tilt.drawing = 1;
  tilt.order = -2;
  SetKey(&posed.pieces["head"], {Frame(0), tilt, Ease::kStep});
  EffectLayer flash{EffectKind::kGlitch, QColor(255, 0, 0, 128), {}};
  SetKey(&flash.amount, {Frame(1), 0.5, Ease::kLinear});
  TextLayer words;
  words.text = "Hello";
  words.fill = Qt::red;

  Shot shot;
  shot.id = ShotId(2);
  shot.name = "Intro";
  shot.length = Frame(72);
  shot.background = Qt::black;
  shot.transition = {TransitionKind::kSwipeUp, Frame(6)};
  CameraPose zoomed;
  zoomed.zoom = 1.5;
  zoomed.shake = 4.0;
  SetKey(&shot.camera, {Frame(3), zoomed, Ease::kEaseInOut});
  shot.layers = {MakeLayer(4, posed), MakeLayer(5, ImageLayer{"/bg.png"}),
                 MakeLayer(6, words), MakeLayer(7, flash)};
  project.shots.push_back(std::make_shared<const Shot>(shot));
  return project;
}

}  // namespace

class ProjectFileTests final : public QObject {
  Q_OBJECT

 private slots:
  void EverythingSurvivesARoundTrip();
  void DamagedFilesSayWhy();
  void PosesSurviveARoundTrip();
};

void ProjectFileTests::EverythingSurvivesARoundTrip() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("a.snapper");
  const Project project = FullProject();
  QVERIFY(WriteProject(path, project).has_value());
  const auto read = ReadProject(path);
  const bool is_read = read.has_value();
  if (!is_read) {
    QFAIL(qPrintable(read.error().message));
  }
  QCOMPARE(read->name, project.name);
  QCOMPARE(read->canvas, project.canvas);
  QCOMPARE(read->song, project.song);
  QCOMPARE(read->next_layer_id, 9);
  QCOMPARE(*read->dolls.at("Bob"), *project.dolls.at("Bob"));
  QCOMPARE(read->shots.size(), size_t{1});
  QCOMPARE(*read->shots[0], *project.shots[0]);
}

void ProjectFileTests::DamagedFilesSayWhy() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("bad.snapper");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("{ not json");
  file.close();
  const auto read = ReadProject(path);
  QVERIFY(!read.has_value());
  QVERIFY(read.error().message.contains("damaged"));
  QVERIFY(!ReadProject(QDir(dir.path()).filePath("none")).has_value());
}

void ProjectFileTests::PosesSurviveARoundTrip() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("poses.json");
  const auto empty = ReadPoses(path);
  QVERIFY(empty.has_value() && empty->empty());
  PiecePose wave;
  wave.rotation = -40.0;
  const std::vector<SavedPose> poses = {{"Wave", "Bob", {{"arm_l", wave}}}};
  QVERIFY(WritePoses(path, poses).has_value());
  const auto read = ReadPoses(path);
  QVERIFY(read.has_value());
  QCOMPARE(*read, poses);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ProjectFileTests)
#include "project_file_tests.moc"
