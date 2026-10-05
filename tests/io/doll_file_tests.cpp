#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

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
  rig.pieces = {{"body", "", {1, 2}, 3, 0, {2, 3}},
                {"head", "body", {4, 5}, 6, -1, {}}};
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

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::DollFileTests)
#include "doll_file_tests.moc"
