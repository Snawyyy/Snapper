#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "io/json_file.h"

namespace snapper {
namespace {

QJsonObject Piece(const QString& name) {
  return QJsonObject{{"name", name},
                     {"drawings", QJsonArray{name + ".png"}},
                     {"position", QJsonArray{0, 0}},
                     {"size", QJsonArray{10, 20}}};
}

// Writes Bob.doll the way the Krita exporter does.
void Export(const QString& library, const QJsonArray& pieces) {
  const QString folder = QDir(library).filePath("Bob.doll");
  QDir().mkpath(folder);
  const auto written = WriteJsonFile(
      QDir(folder).filePath("art.json"),
      QJsonObject{{"format", "snapper-art"},
                  {"version", 2},
                  {"canvas", QJsonArray{100, 100}},
                  {"pieces", pieces}});
  QVERIFY(written.has_value());
}

void UseBob(HistoryManager* history) {
  Project next = history->current();
  Shot shot;
  shot.id = ShotId(1);
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Bob", {}, false};
  shot.layers.push_back(layer);
  next.shots.push_back(std::make_shared<const Shot>(shot));
  history->Commit("Use", next);
}

}  // namespace

class LibraryTests final : public QObject {
  Q_OBJECT

 private slots:
  void ImportsOnce();
  void ReloadKeepsTheRig();
  void SavedRigGoesBackToTheLibrary();
  void RemovingAUsedDollIsRefused();
  void NoticesReexports();
};

void LibraryTests::ImportsOnce() {
  QTemporaryDir dir;
  Export(dir.path(), {Piece("body")});
  HistoryManager history{Project()};
  DollLibraryManager library(&history, dir.path());
  QCOMPARE(library.Available(), QStringList({"Bob"}));
  const auto report = library.Import("Bob");
  QVERIFY(report.has_value());
  QCOMPARE(report->added, QStringList({"body"}));
  QVERIFY(FindDoll(history.current(), "Bob") != nullptr);
  QCOMPARE(history.UndoLabel(), QString("Import Bob"));
  QVERIFY(!library.Import("Bob").has_value());
  QVERIFY(!library.Import("Ann").has_value());
}

void LibraryTests::ReloadKeepsTheRig() {
  QTemporaryDir dir;
  Export(dir.path(), {Piece("body")});
  HistoryManager history{Project()};
  DollLibraryManager library(&history, dir.path());
  QVERIFY(library.Import("Bob").has_value());
  Project rigged = history.current();
  Doll doll = *FindDoll(rigged, "Bob");
  doll.rig.pieces[0].pivot = QPointF(1, 2);
  rigged.dolls["Bob"] = std::make_shared<const Doll>(doll);
  history.Commit("Pivot", rigged);
  Export(dir.path(), {Piece("body"), Piece("hat")});
  const auto report = library.Reload("Bob");
  QVERIFY(report.has_value());
  QCOMPARE(report->added, QStringList({"hat"}));
  const Doll* fresh = FindDoll(history.current(), "Bob");
  QCOMPARE(fresh->art.pieces.size(), size_t{2});
  QCOMPARE(FindRig(fresh->rig, "body")->pivot, QPointF(1, 2));
  QVERIFY(!library.Reload("Ann").has_value());
}

void LibraryTests::SavedRigGoesBackToTheLibrary() {
  QTemporaryDir dir;
  Export(dir.path(), {Piece("body")});
  HistoryManager history{Project()};
  DollLibraryManager library(&history, dir.path());
  QVERIFY(library.Import("Bob").has_value());
  QVERIFY(library.SaveRig("Bob").has_value());
  const auto rig = ReadRig(QDir(dir.path()).filePath("Bob.doll"));
  QVERIFY(rig.has_value());
  QCOMPARE(rig->pieces.size(), size_t{1});
}

void LibraryTests::RemovingAUsedDollIsRefused() {
  QTemporaryDir dir;
  Export(dir.path(), {Piece("body")});
  HistoryManager history{Project()};
  DollLibraryManager library(&history, dir.path());
  QVERIFY(library.Import("Bob").has_value());
  UseBob(&history);
  QCOMPARE(library.UseCount("Bob"), 1);
  QVERIFY(!library.Remove("Bob").has_value());
  history.Undo();
  QVERIFY(library.Remove("Bob").has_value());
  QVERIFY(FindDoll(history.current(), "Bob") == nullptr);
}

void LibraryTests::NoticesReexports() {
  QTemporaryDir dir;
  Export(dir.path(), {Piece("body")});
  HistoryManager history{Project()};
  DollLibraryManager library(&history, dir.path());
  QVERIFY(library.Import("Bob").has_value());
  QSignalSpy changed(&library, &DollLibraryManager::ArtChanged);
  QTest::qWait(20);
  // The exporter swaps the whole folder in.
  QVERIFY(QDir(QDir(dir.path()).filePath("Bob.doll")).removeRecursively());
  Export(dir.path(), {Piece("body"), Piece("hat")});
  QVERIFY(changed.wait(2000));
  QCOMPARE(changed.first().first().toString(), QString("Bob"));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::LibraryTests)
#include "library_tests.moc"
