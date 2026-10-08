#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

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
  CameraPose zoomed;
  zoomed.zoom = 1.5;
  zoomed.shake = 4.0;
  SetKey(&shot.camera, {Frame(3), zoomed, Ease::kEaseInOut});
  shot.layers = {MakeLayer(4, posed), MakeLayer(5, ImageLayer{"/bg.png"}),
                 MakeLayer(6, words), MakeLayer(7, flash)};
  project.shots.push_back(std::make_shared<const Shot>(shot));

  project.next_clip_id = 4;
  Clip paint;
  paint.id = ClipId(1);
  paint.source = VideoSource{"/takes/paint.mp4", Frame(900)};
  paint.in = Frame(30);
  paint.length = Frame(600);
  paint.out = {TransitionKind::kSwipeUp, Frame(6)};
  Clip intro;
  intro.id = ClipId(3);
  intro.source = ShotSource{ShotId(2)};
  intro.start = Frame(120);
  intro.length = Frame(72);
  PlaceClip(&project.reel.tracks[0], paint);
  PlaceClip(&project.reel.tracks[2], intro);
  project.reel.markers = {Frame(24), Frame(96)};
  return project;
}

// Writes object as a project file at path.
bool WriteObject(const QString& path, const QJsonObject& object) {
  assert(!path.isEmpty());
  assert(!object.isEmpty());
  QFile file(path);
  const bool is_open = file.open(QIODevice::WriteOnly);
  return is_open && file.write(QJsonDocument(object).toJson()) > 0;
}

}  // namespace

class ProjectFileTests final : public QObject {
  Q_OBJECT

 private slots:
  void EverythingSurvivesARoundTrip();
  void DamagedFilesSayWhy();
  void OldFilesGetAnEmptyReel();
  void OverlappingClipsAreDamage();
  void OldShotTransitionsMoveToTheReel();
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
  QCOMPARE(read->next_clip_id, 4);
  QCOMPARE(read->reel, project.reel);
}

void ProjectFileTests::DamagedFilesSayWhy() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("bad.snapper");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("not json at all");
  file.close();
  const auto read = ReadProject(path);
  QVERIFY(!read.has_value());
  QVERIFY(read.error().message.contains("damaged"));
  QVERIFY(!ReadProject(QDir(dir.path()).filePath("none")).has_value());
}

void ProjectFileTests::OldFilesGetAnEmptyReel() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("old.snapper");
  QVERIFY(WriteObject(path, {{"format", "snapper-project"},
                             {"version", 1},
                             {"canvas", QJsonArray{1920, 1080}}}));
  const auto read = ReadProject(path);
  QVERIFY(read.has_value());
  QCOMPARE(read->reel, Reel());
  QCOMPARE(read->next_clip_id, 1);
}

void ProjectFileTests::OverlappingClipsAreDamage() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("overlap.snapper");
  const QJsonObject first{{"id", 1}, {"start", 0}, {"length", 10},
                          {"shot", 1}};
  const QJsonObject second{{"id", 2}, {"start", 5}, {"length", 10},
                           {"shot", 1}};
  const QJsonArray reel{QJsonArray{first, second}};
  QVERIFY(WriteObject(path, {{"format", "snapper-project"},
                             {"version", 2},
                             {"canvas", QJsonArray{1920, 1080}},
                             {"reel", reel}}));
  const auto read = ReadProject(path);
  QVERIFY(!read.has_value());
  QVERIFY(read.error().message.contains("overlap"));
}

void ProjectFileTests::OldShotTransitionsMoveToTheReel() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("shots.snapper");
  const QJsonObject fade{{"kind", "crossfade"}, {"length", 4}};
  const QJsonObject flash{{"kind", "flash"}, {"length", 2}};
  const QJsonObject one{{"id", 1}, {"length", 48}, {"transition", fade}};
  const QJsonObject two{{"id", 2}, {"length", 48}, {"transition", flash}};
  const QJsonArray shots{one, two};
  // Shot 1 meets shot 2 on the bottom track and stands alone above;
  // shot 2 has nothing after it.
  const QJsonObject a{{"id", 1}, {"start", 0}, {"length", 48}, {"shot", 1}};
  const QJsonObject b{{"id", 2}, {"start", 48}, {"length", 48}, {"shot", 2}};
  const QJsonObject c{{"id", 3}, {"start", 200}, {"length", 48},
                      {"shot", 1}};
  const QJsonArray bottom{a, b};
  const QJsonArray top{c};
  QVERIFY(WriteObject(path, {{"format", "snapper-project"},
                             {"version", 2},
                             {"canvas", QJsonArray{1920, 1080}},
                             {"shots", shots},
                             {"reel", QJsonArray{bottom, top}}}));
  const auto read = ReadProject(path);
  QVERIFY(read.has_value());
  QCOMPARE(ClipOf(read->reel, ClipId(1))->out,
           (Transition{TransitionKind::kCrossfade, Frame(4)}));
  QCOMPARE(ClipOf(read->reel, ClipId(2))->out, Transition());
  QCOMPARE(ClipOf(read->reel, ClipId(3))->out, Transition());
  // Saved again, the shots carry no transition.
  QVERIFY(WriteProject(path, *read).has_value());
  QFile saved(path);
  QVERIFY(saved.open(QIODevice::ReadOnly));
  QVERIFY(!saved.readAll().contains("flash"));
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
