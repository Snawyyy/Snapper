#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

#include "io/doll_file.h"
#include "io/json_file.h"

namespace snapper {
namespace {

void Write(const QString& path, const QJsonObject& object) {
  QVERIFY(WriteJsonFile(path, object).has_value());
}

QJsonObject TwoPieceArt() {
  return QJsonObject{
      {"format", "snapper-art"},
      {"version", 2},
      {"canvas", QJsonArray{100, 80}},
      {"pieces",
       QJsonArray{
           QJsonObject{{"name", "body"},
                       {"drawings", QJsonArray{"body_0.png"}},
                       {"position", QJsonArray{-50, -40}},
                       {"size", QJsonArray{100, 80}}},
           QJsonObject{{"name", "head"},
                       {"drawings", QJsonArray{"head_0.png", "head_1.png"}},
                       {"default", 1},
                       {"position", QJsonArray{-10, -30}},
                       {"size", QJsonArray{20, 10}}}}}};
}

}  // namespace

class DollFileTests final : public QObject {
  Q_OBJECT

 private slots:
  void ReadsArtAndGivesNewPiecesADefaultRig();
  void RigSurvivesARoundTrip();
  void ReexportKeepsTheRigAndReportsChanges();
  void ReadsTheOldExporterFormat();
  void RefusesForeignFiles();
  void KeepsTheOldSnappersRig();
  void SpreadsAnOldPieceMotionOverItsPoints();
};

void DollFileTests::ReadsArtAndGivesNewPiecesADefaultRig() {
  QTemporaryDir dir;
  Write(QDir(dir.path()).filePath("art.json"), TwoPieceArt());
  const auto loaded = LoadDoll(dir.path());
  QVERIFY(loaded.has_value());
  const Doll& doll = loaded->doll;
  QCOMPARE(doll.art.pieces[1].drawings.size(), size_t{2});
  QCOMPARE(doll.art.pieces[1].default_drawing, 1);
  QCOMPARE(doll.rig.pieces[1].pivot, QPointF(10, 5));
  QCOMPARE(doll.rig.pieces[1].order, 1);
  QCOMPARE(loaded->report.added, QStringList({"body", "head"}));
}

void DollFileTests::RigSurvivesARoundTrip() {
  QTemporaryDir dir;
  Rig rig;
  rig.pieces = {{"body", "", {1, 2}, 3, 0, {1, 1}, 0.0, false,
                 {0.0, 1.5, 0.0, 2.0}, {{3, 0.25, 0.75}},
                 {{1, WarpMotionKind::kPulse, 12.0, 30, -45.0, 0.25},
                  {3, WarpMotionKind::kShiver, 2.0, 6, 0.0, 0.0}}},
                {"head", "body", {4, 5}, 6, -1, {}, 10.0, true}};
  rig.chains = {{"neck", "body", "head", {7, 8}, false}};
  QVERIFY(WriteRig(dir.path(), rig).has_value());
  const auto read = ReadRig(dir.path());
  QVERIFY(read.has_value());
  QCOMPARE(*read, rig);
}

void DollFileTests::ReexportKeepsTheRigAndReportsChanges() {
  DollArt art;
  art.pieces = {{"body", {"b.png"}, 0, {}, {10, 10}},
                {"hat", {"h.png"}, 0, {}, {4, 4}}};
  Rig rig;
  rig.pieces = {{"body", "arm", {1, 1}, 9, 3, {}},
                {"arm", "", {0, 0}, 1, -1, {}}};
  rig.chains = {{"reach", "arm", "body", {}, true}};
  ReconcileReport report;
  const Rig fitted = Reconcile(art, rig, &report);
  QCOMPARE(fitted.pieces.size(), size_t{2});
  QCOMPARE(fitted.pieces[0].pivot, QPointF(1, 1));
  QCOMPARE(fitted.pieces[0].order, 9);
  QVERIFY(fitted.pieces[0].parent.isEmpty());
  QCOMPARE(fitted.pieces[0].default_drawing, -1);
  QCOMPARE(report.added, QStringList({"hat"}));
  QCOMPARE(report.missing, QStringList({"arm"}));
  QCOMPARE(report.dropped_chains, QStringList({"reach"}));
  QVERIFY(fitted.chains.empty());
}

void DollFileTests::ReadsTheOldExporterFormat() {
  QTemporaryDir dir;
  const QJsonObject legacy{
      {"version", 1},
      {"pieces", QJsonArray{QJsonObject{
                     {"name", "head"},
                     {"drawings", QJsonArray{"piece1_0.png"}},
                     {"pin_x", 0.0},
                     {"pin_y", -25.0},
                     {"pivot_x", 10.0},
                     {"pivot_y", 5.0}}}}};
  Write(QDir(dir.path()).filePath("doll.json"), legacy);
  const auto art = ReadArt(dir.path());
  QVERIFY(art.has_value());
  QCOMPARE(art->pieces[0].position, QPointF(-10, -30));
  QCOMPARE(art->pieces[0].size, QSize(20, 10));
}

void DollFileTests::RefusesForeignFiles() {
  QTemporaryDir dir;
  Write(QDir(dir.path()).filePath("art.json"),
        QJsonObject{{"format", "something-else"}, {"version", 1}});
  const auto art = ReadArt(dir.path());
  QVERIFY(!art.has_value());
  QVERIFY(art.error().message.contains("not a doll file"));
  QVERIFY(!ReadArt(QDir(dir.path()).filePath("missing")).has_value());
}

void DollFileTests::KeepsTheOldSnappersRig() {
  QTemporaryDir dir;
  QImage drawing(30, 40, QImage::Format_ARGB32);
  drawing.fill(Qt::red);
  QVERIFY(drawing.save(QDir(dir.path()).filePath("arm.png")));
  const QJsonObject legacy{
      {"version", 1},
      {"pieces",
       QJsonArray{
           QJsonObject{{"id", 1}, {"name", "body"}, {"parent", 0},
                       {"drawings", QJsonArray{"body.png"}},
                       {"pin_x", 5.0}, {"pin_y", 5.0},
                       {"pivot_x", 10.0}, {"pivot_y", 20.0},
                       {"order", 0}, {"rest_rotation", 0.0}},
           QJsonObject{{"id", 2}, {"name", "arm"}, {"parent", 1},
                       {"drawings", QJsonArray{"arm.png"}},
                       {"pin_x", 18.0}, {"pin_y", 4.0},
                       {"pivot_x", 2.0}, {"pivot_y", 3.0},
                       {"order", 3}, {"rest_rotation", 15.0}}}}};
  Write(QDir(dir.path()).filePath("doll.json"), legacy);
  const auto loaded = LoadDoll(dir.path());
  QVERIFY(loaded.has_value());
  const Doll& doll = loaded->doll;
  QCOMPARE(FindArt(doll, "body")->position, QPointF(-5, -15));
  QCOMPARE(FindArt(doll, "arm")->position, QPointF(11, -14));
  QCOMPARE(FindArt(doll, "arm")->size, QSize(30, 40));
  const RigPiece* arm = FindRig(doll.rig, "arm");
  QCOMPARE(arm->parent, QString("body"));
  QCOMPARE(arm->pivot, QPointF(2, 3));
  QCOMPARE(arm->order, 3);
  QCOMPARE(arm->rest_rotation, 15.0);
  QVERIFY(loaded->report.added.isEmpty());
}

void DollFileTests::SpreadsAnOldPieceMotionOverItsPoints() {
  QTemporaryDir dir;
  // Before point motions, a motion moved a piece's whole grid.
  const QJsonObject wave{{"kind", "wave"}, {"size", 10.0}, {"cycle", 30},
                         {"edge", "top"}};
  const QJsonObject pulse{{"kind", "pulse"}, {"size", 4.0}, {"cycle", 24},
                          {"edge", "left"}};
  const QJsonObject rig{
      {"format", "snapper-rig"},
      {"version", 1},
      {"pieces",
       QJsonArray{QJsonObject{{"name", "hair"}, {"warp", QJsonArray{1, 2}},
                              {"motion", wave}},
                  QJsonObject{{"name", "heart"}, {"warp", QJsonArray{2, 2}},
                              {"motion", pulse}}}}};
  Write(QDir(dir.path()).filePath("rig.json"), rig);
  const auto read = ReadRig(dir.path());
  QVERIFY(read.has_value());
  // The top row hangs still; lower rows swing wider and later.
  const std::vector<PointMotion>& hair = read->pieces[0].point_motions;
  QCOMPARE(hair.size(), size_t{4});
  QCOMPARE(hair[0], (PointMotion{2, WarpMotionKind::kWave, 5.0, 30, 0.0,
                                 0.5}));
  QCOMPARE(hair[3], (PointMotion{5, WarpMotionKind::kWave, 10.0, 30, 0.0,
                                 1.0}));
  // The middle stays; the rest beat out from it.
  const std::vector<PointMotion>& heart = read->pieces[1].point_motions;
  QCOMPARE(heart.size(), size_t{8});
  QCOMPARE(heart[0].point, 0);
  QCOMPARE(heart[0].angle, -135.0);
  QVERIFY(std::abs(heart[0].size - 4.0 * std::sqrt(2.0)) < 1e-9);
  QCOMPARE(heart[4].point, 5);
  QCOMPARE(heart[4].angle, 0.0);
  QCOMPARE(heart[4].size, 4.0);
  // Saved again, the file holds point motions only.
  QVERIFY(WriteRig(dir.path(), *read).has_value());
  QCOMPARE(ReadRig(dir.path())->pieces[1].point_motions, heart);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::DollFileTests)
#include "doll_file_tests.moc"
